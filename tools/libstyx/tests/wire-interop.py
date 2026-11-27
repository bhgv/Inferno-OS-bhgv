# Copyright (c) 2026 bhgv
# EN: Cross-feed independently encoded 9P2000 packets through both original C codec sets.
# RU: Передавать независимые пакеты 9P2000 через оба комплекта исходных C-кодеков.
import argparse
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile


# EN/RU: Encode fixed-width wire values in protocol order, independent of native ABI.
# Кодировать фиксированные значения в wire-порядке независимо от native ABI.
def pack(format, *values):
    return struct.pack('<' + format, *values)


# EN/RU: Count UTF-8 bytes, never characters or native pointer width.
# Считать байты UTF-8, а не символы или размер указателя.
def string(text):
    data = text.encode('utf-8')
    return pack('H', len(data)) + data


# EN/RU: Produce every legal base message plus negotiation variants and high-bit fields.
# Создать все допустимые base-сообщения, варианты версии и поля со старшими битами.
def corpus():
    fid, other = 0xf1234567, 0x80010001
    qid = pack('BIQ', 0x80, 0xabcdef12, 0xfedcba9876543210)
    stat = pack('HI', 0x1234, 0x89abcdef) + qid
    stat += pack('IIIQ', 0x800001ed, 0xf1234567, 0x81234567, 0x123456789abcdef)
    stat += b''.join(string(s) for s in ('folder-λ', 'alice', 'group', 'bob'))
    stat = pack('H', len(stat)) + stat
    fields = {
        100: pack('I', 8192) + string('9P2000'),
        101: pack('I', 4096) + string('9P2000'),
        102: pack('I', other) + string('alice') + string('root'),
        103: qid,
        104: pack('II', fid, 0xffffffff) + string('alice') + string('root'),
        105: qid,
        107: string('error-λ'),
        108: pack('H', 0xf123),
        109: b'',
        110: pack('IIH', fid, other, 2) + string('first') + string('later-λ'),
        111: pack('H', 2) + qid + qid,
        112: pack('IB', fid, 2),
        113: qid + pack('I', 4096),
        114: pack('I', fid) + string('file-λ') + pack('IB', 0x800001ed, 2),
        115: qid + pack('I', 4096),
        116: pack('IQI', fid, 0x123456789abcdef, 0x80010001),
        117: pack('I', len(stat)) + stat,
        118: pack('IQI', fid, 0x123456789abcdef, 4) + b'a\x00\xffz',
        119: pack('I', 0x80010001),
        120: pack('I', fid),
        121: b'',
        122: pack('I', fid),
        123: b'',
        124: pack('I', fid),
        125: pack('H', len(stat)) + stat,
        126: pack('IH', fid, len(stat)) + stat,
        127: b'',
    }
    packets = [pack('IBH', 7 + len(body), kind, 65535 if kind == 100 else 0xf123) + body
               for kind, body in fields.items()]
    for kind, body in [(100, pack('I', 1024) + string('9P2000.u')),
                       (101, pack('I', 1024) + string('unknown'))]:
        packets.append(pack('IBH', 7 + len(body), kind, 65535) + body)
    return b''.join(packets)


# EN/RU: Build each codec set separately; cross-feed resulting bytes in both directions.
# Собрать кодеки отдельно и передать полученные байты в обе стороны.
# --lib9 supplies native support only; all four tested codec units are compiled from each root.
# --lib9 даёт native-поддержку; четыре проверяемых codec-файла компилируются из каждого корня.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--peer', required=True, type=Path, help='FlashVM checkout')
    parser.add_argument('--lib9', required=True, type=Path, help='native support archive for target architecture')
    parser.add_argument('--arch', default='amd64', choices=('amd64', 'arm64', 'riscv64'))
    parser.add_argument('--runner', default='')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    peer = args.peer.resolve() / 'disVM/native/src'
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='styx-wire-') as name:
        work = Path(name)
        original = work / 'original'
        original.write_bytes(corpus())
        executables = []
        for label, source in [('inferno', root), ('disvm', peer)]:
            executable = work / label
            codecs = [str(source / 'lib9' / (unit + '.c'))
                      for unit in ('convM2S', 'convS2M', 'convM2D', 'convD2M')]
            subprocess.run(compiler + flags + ['-I' + str(source / 'Linux' / args.arch / 'include'),
                '-I' + str(source / 'include'), str(root / 'tools/libstyx/tests/wire-interop.c')]
                + codecs + [str(args.lib9.resolve()), '-lm', '-lpthread', '-o', str(executable)], check=True)
            executables.append(executable)
        for order in (executables, executables[::-1]):
            incoming = original
            for index, executable in enumerate(order):
                outgoing = work / ('out-' + executable.name + str(index))
                subprocess.run(shlex.split(args.runner) + [str(executable), str(incoming), str(outgoing)], check=True)
                if outgoing.read_bytes() != original.read_bytes():
                    raise RuntimeError('cross-VM wire mismatch')
                incoming = outgoing
        print('PASS Inferno <-> DisVM original C Styx wire:', args.arch)


if __name__ == '__main__':
    main()
