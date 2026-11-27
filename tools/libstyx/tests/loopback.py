# Copyright (c) 2026 bhgv
# EN: Independent 9P test packets drive the real C server over two native TCP connections.
# RU: Независимые тестовые пакеты 9P проверяют C-сервер через два TCP-соединения.
import selectors
import socket
import struct
import subprocess
import sys


# EN/RU: Encode a counted fixture string, without implementing a production client.
# Кодировать тестовую строку; это не production-клиент.
def string(text):
    data = text.encode()
    return struct.pack('<H', len(data)) + data


# EN/RU: Read exactly n wire bytes; EOF is an explicit test failure.
# Прочитать ровно n байтов; EOF означает отказ теста.
def exact(sock, n):
    data = b''
    while len(data) < n:
        part = sock.recv(n - len(data))
        if not part:
            raise RuntimeError('unexpected TCP EOF')
        data += part
    return data


# EN/RU: Send one independent wire vector and validate type, tag and negotiated bound.
# Послать независимый вектор и проверить тип, tag и предел размера.
def rpc(sock, kind, payload, tag=9, limit=1024):
    packet = struct.pack('<IBH', 7 + len(payload), kind, tag) + payload
    sock.sendall(packet[:3])
    sock.sendall(packet[3:])
    size = struct.unpack('<I', exact(sock, 4))[0]
    if not 7 <= size <= limit:
        raise RuntimeError(f'invalid reply size {size}')
    body = exact(sock, size - 4)
    reply_kind, reply_tag = struct.unpack_from('<BH', body)
    if reply_kind != kind + 1 or reply_tag != tag:
        raise RuntimeError(f'unexpected reply {reply_kind}/{reply_tag}: {body!r}')
    return body[3:]


# EN/RU: Prepare a connection and retain an open directory fid, with independent limits.
# Подготовить соединение и сохранить открытый fid каталога с отдельным лимитом.
def connection(port, user, limit):
    sock = socket.create_connection(('127.0.0.1', port), timeout=5)
    sock.settimeout(5)
    reply = rpc(sock, 100, struct.pack('<I', limit) + string('9P2000'), 65535, limit)
    if struct.unpack_from('<I', reply)[0] != limit:
        raise RuntimeError('negotiated limit mismatch')
    rpc(sock, 104, struct.pack('<II', 1, 0xffffffff) + string(user) + string(''), limit=limit)
    rpc(sock, 110, struct.pack('<IIH', 1, 2, 0), limit=limit)
    rpc(sock, 112, struct.pack('<IB', 2, 0), limit=limit)
    return sock


# EN/RU: Read a file/directory response using the actual byte offset.
# Прочитать ответ файла/каталога по настоящему байтовому смещению.
def read(sock, fid, offset=0, limit=1024):
    reply = rpc(sock, 116, struct.pack('<IQI', fid, offset, 65536), limit=limit)
    count = struct.unpack_from('<I', reply)[0]
    if count != len(reply) - 4:
        raise RuntimeError('read count mismatch')
    return reply[4:]


# EN/RU: Decode one stat record independently and check all counted strings fit.
# Независимо разобрать stat и проверить границы всех строк.
def stat(record):
    if len(record) < 49 or struct.unpack_from('<H', record)[0] != len(record) - 2:
        raise RuntimeError('incomplete stat')
    offset, fields = 41, []
    for _ in range(4):
        n = struct.unpack_from('<H', record, offset)[0]
        offset += 2
        fields.append(record[offset:offset+n].decode())
        offset += n
    if offset != len(record):
        raise RuntimeError('trailing stat bytes')
    return fields, struct.unpack_from('<I', record, 21)[0], struct.unpack_from('<Q', record, 13)[0]


# EN/RU: Walk from the retained root and open one fixture endpoint.
# Пройти от сохранённого корня и открыть тестовый endpoint.
def openfile(sock, fid, name, mode):
    reply = rpc(sock, 110, struct.pack('<IIH', 1, fid, 1) + string(name))
    if struct.unpack_from('<H', reply)[0] != 1:
        raise RuntimeError('walk did not resolve one name')
    rpc(sock, 112, struct.pack('<IB', fid, mode))


# EN/RU: Exercise live isolated views and close the actual native server deterministically.
# Проверить живые изолированные представления и детерминированно закрыть C-сервер.
def main():
    child = subprocess.Popen([sys.argv[1]], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    sockets = []
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(child.stdout, selectors.EVENT_READ)
            if not ready.select(5):
                raise RuntimeError('server did not announce')
        port = int(child.stdout.readline())
        alice = connection(port, 'alice', 1024)
        sockets.append(alice)
        bob = connection(port, 'bob', 512)
        sockets.append(bob)
        initial = read(alice, 2)
        fields, mode, qid = stat(initial)
        if fields[0] != 'first' or fields[1] != 'alice' or mode != 0o444 or qid != 0xfedcba9876543210:
            raise RuntimeError('initial live attributes')
        if read(alice, 2, len(initial)) != b'':
            raise RuntimeError('directory EOF offset')
        openfile(alice, 3, 'first', 0)
        openfile(alice, 4, 'ctl', 1)
        rpc(alice, 118, struct.pack('<IQI', 4, 0, 6) + b'change')
        fields, mode, _ = stat(read(alice, 2))
        if fields[0] != 'later' or mode != 0o640 or read(alice, 3) != b'changed':
            raise RuntimeError('retained fids did not observe live change')
        fields, mode, _ = stat(read(bob, 2, limit=512))
        if fields[0] != 'bob' or fields[1] != 'bob' or mode != 0o444:
            raise RuntimeError('cross-connection view contamination')
        record = rpc(alice, 124, struct.pack('<I', 3))
        if struct.unpack_from('<H', record)[0] != len(record)-2 or stat(record[2:])[0][0] != 'later':
            raise RuntimeError('live stat')
        bob.close()
        rpc(alice, 118, struct.pack('<IQI', 4, 0, 4) + b'stop')
        output, errors = child.communicate(timeout=5)
        if child.returncode != 0 or 'closed 4 handles, released 2 clients' not in output:
            raise RuntimeError(f'server cleanup {child.returncode}: {output} {errors}')
        print('PASS original C POSIX TCP live views and cleanup')
    finally:
        for sock in sockets:
            sock.close()
        if child.poll() is None:
            child.terminate()
            child.communicate(timeout=5)


if __name__ == '__main__':
    main()
