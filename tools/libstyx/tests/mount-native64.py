# Copyright (c) 2026 bhgv
# EN: Temporarily instrument a prepared emu build to test its real kernel mount client.
# RU: Временно инструментировать подготовленный emu для проверки kernel mount-клиента.
import argparse
from contextlib import contextmanager
from pathlib import Path
import runpy
import selectors
import shlex
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import time


# EN/RU: Split the first real server reply into single-byte sends, then relay transparently.
# Разбить первый ответ настоящего сервера на байты, затем передавать поток прозрачно.
@contextmanager
def fragmented_version(port):
    errors = []
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(10)
        def relay():
            try:
                client, _ = listener.accept()
                with client, socket.create_connection(('127.0.0.1', port), timeout=10) as server:
                    client.settimeout(10)
                    server.settimeout(10)
                    client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    def requests():
                        try:
                            while True:
                                data = client.recv(4096)
                                if not data:
                                    server.shutdown(socket.SHUT_WR)
                                    return
                                server.sendall(data)
                        except OSError as error:
                            errors.append(str(error))
                    sender = threading.Thread(target=requests)
                    sender.start()
                    def exact(n):
                        data = b''
                        while len(data) < n:
                            part = server.recv(n - len(data))
                            if not part:
                                raise RuntimeError('EOF in first version reply')
                            data += part
                        return data
                    header = exact(4)
                    size = struct.unpack('<I', header)[0]
                    if not 7 <= size <= 1024:
                        raise RuntimeError('invalid first reply size')
                    first = header + exact(size - 4)
                    for byte in first:
                        client.sendall(bytes([byte]))
                        time.sleep(0.002)
                    while True:
                        data = server.recv(4096)
                        if not data:
                            break
                        client.sendall(data)
                    sender.join(5)
                    if sender.is_alive():
                        raise RuntimeError('request relay did not retire')
            except Exception as error:
                errors.append(str(error))
        worker = threading.Thread(target=relay)
        worker.start()
        try:
            yield listener.getsockname()[1]
        finally:
            worker.join(15)
            if worker.is_alive() or errors:
                raise RuntimeError('fragment relay retirement: ' + repr(errors))


# EN/RU: Run the native client against the original fixture, then check server ownership cleanup.
# Запустить native-клиент против исходного fixture и проверить освобождение серверных владельцев.
def exchange(command, root, server_path, fragment=False):
    source = Path(__file__).resolve().parents[3]
    wire = runpy.run_path(str(source / 'tools/libstyx/tests/loopback.py'))
    boot = runpy.run_path(str(source / 'utils/tests/native64-scalar-boot.py'))
    server = subprocess.Popen([str(server_path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            if not ready.select(10):
                raise RuntimeError('Styx fixture did not announce its port')
        port = int(server.stdout.readline())
        def run(client_port):
            return boot['execute'](command + ['-v', '-r', str(root), '-d', 'tcp!127.0.0.1!' + str(client_port)])
        if fragment:
            with fragmented_version(port) as proxy_port:
                output = run(proxy_port)
        else:
            output = run(port)
        if 'PASS native64 kernel Styx mount:' not in output or 'panic:' in output or 'faults:' in output:
            raise RuntimeError(output)
        print(output, end='', flush=True)
        # The native client closes three handles and its connection. A second
        # connection opens just ctl, so the original fixture expects four/two.
        with socket.create_connection(('127.0.0.1', port), timeout=5) as peer:
            wire['rpc'](peer, 100, struct.pack('<I', 1024) + wire['string']('9P2000'), 65535)
            wire['rpc'](peer, 104, struct.pack('<II', 1, 0xffffffff) + wire['string']('alice') + wire['string'](''))
            wire['openfile'](peer, 3, 'ctl', 1)
            wire['rpc'](peer, 118, struct.pack('<IQI', 3, 0, 4) + b'stop')
        output, errors = server.communicate(timeout=10)
        if server.returncode or 'closed 4 handles, released 2 clients' not in output:
            raise RuntimeError('Styx fixture cleanup: ' + output + errors)
        print(output, end='', flush=True)
    finally:
        if server.poll() is None:
            server.terminate()
            server.communicate(timeout=5)


# EN/RU: Patch only the disposable build's boot translation unit, restore and rebuild in finally.
# Изменять boot-файл только подготовленной сборки; восстановить и пересобрать в finally.
# The --root tree must already contain current production devmnt.c and an existing /mnt.
# Дерево --root должно содержать текущий devmnt.c и существующий /mnt.
def main():
    parser = argparse.ArgumentParser()
    for name in ('root', 'emu', 'server'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--runner', default='')
    parser.add_argument('--build-command', default='mk CONF=emu-g install')
    args = parser.parse_args()
    root = args.root.resolve()
    source = Path(__file__).resolve().parents[3]
    target = root / 'emu/port/dis.c'
    original = target.read_bytes()
    if (root / 'emu/port/devmnt.c').read_bytes() != (source / 'emu/port/devmnt.c').read_bytes():
        raise RuntimeError('prepared build has stale devmnt.c')
    if not (root / 'mnt').is_dir():
        raise RuntimeError('prepared build needs /mnt directory')
    text = original.decode()
    marker = 'void\ndisinit(void *a)'
    if text.count(marker) != 1 or text.count('\texcinit();') != 1:
        raise RuntimeError('unexpected boot source')
    text = text.replace(marker, '#include "' + str(source / 'tools/libstyx/tests/mount-native64.c') + '"\n' + marker)
    text = text.replace('\texcinit();', '\texcinit();\n\tmountacceptance(initmod);')
    build = shlex.split(args.build_command)
    with tempfile.TemporaryDirectory(prefix='native64-mount-') as name:
        work = Path(name)
        def rebuild(log):
            with (work / log).open('w') as output:
                result = subprocess.run(build, cwd=root / 'emu/Linux', stdout=output, stderr=subprocess.STDOUT)
            if result.returncode:
                raise RuntimeError((work / log).read_text())
        try:
            target.write_text(text)
            rebuild('probe-build.log')
            executable = work / 'emu-mount'
            shutil.copy2(args.emu.resolve(), executable)
            for fragment in (False, True):
                exchange(shlex.split(args.runner) + [str(executable)], root, args.server.resolve(), fragment)
                print("PASS live mount fragmented version=" + str(fragment), flush=True)
        finally:
            target.write_bytes(original)
            rebuild('restore-build.log')
    print('PASS real native64 kernel mount and restored ordinary emu', flush=True)


if __name__ == '__main__':
    main()
