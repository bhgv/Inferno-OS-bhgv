# Copyright (c) 2026 bhgv
# EN: Real TCP deferred-read/producer wakeup and standard9P cancellation acceptance.
# RU: Real TCP deferred-read/producer wakeup и standard9P cancellation acceptance.
import selectors
import socket
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
from loopback import connection, openfile, rpc, exact, string


# EN/RU: Bounded external readiness checkpoint, no sleeps or guest progress.
# Ограниченный external readiness checkpoint без sleeps/guest progress.
def line(stream):
    with selectors.DefaultSelector() as ready:
        ready.register(stream, selectors.EVENT_READ)
        if not ready.select(5):
            raise RuntimeError('native owner checkpoint deadline')
    return stream.readline().strip()


# EN/RU: Independent fixture packet; production code still uses original Inferno codecs.
# Независимый fixture packet; production code использует исходные Inferno codecs.
def send(sock, kind, tag, payload):
    sock.sendall(struct.pack('<IBH', 7+len(payload), kind, tag)+payload)


# EN/RU: Decode one complete reply, retaining original tag and exact payload length.
# Decode один полный ответ с original tag и exact payload length.
def receive(sock, kind, tag):
    size = struct.unpack('<I', exact(sock, 4))[0]
    packet = exact(sock, size-4)
    if size < 7 or size > 1024 or struct.unpack_from('<BH', packet) != (kind, tag):
        raise RuntimeError(f'unexpected deferred response: {size}/{packet!r}')
    return packet[3:]


# EN/RU: Wait for actual server admission, before external producer emits any record.
# Дождаться actual server admission до публикации record external producer.
def pending(server, sock, tag=50, count=16):
    send(sock, 116, tag, struct.pack('<IQI', 3, 0, count))
    if line(server.stdout) != 'pending':
        raise RuntimeError('server did not defer Tread')


# EN/RU: Assert exactly one fixture event in one Rread; no aggregate queue dump.
# Проверить ровно одну fixture event в Rread, без aggregate queue dump.
def record(sock, tag, value):
    payload = receive(sock, 117, tag)
    if payload != struct.pack('<I', len(value))+value:
        raise RuntimeError(f'wrong one-record reply {payload!r}')


# EN/RU: Drive native producer through its own external fixture pipe, not a guest callback.
# Управлять native producer через external fixture pipe, не guest callback.
def emit(server, commands='e'):
    server.stdin.write(commands)
    server.stdin.flush()


# EN/RU: Exercise unchanged wire protocol against actual original C POSIX server.
# Проверить неизменённый wire protocol с настоящим исходным C POSIX server.
def main():
    mode = sys.argv[2]
    server = subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    sock = None
    try:
        port = int(line(server.stdout))
        sock = connection(port, 'alice', 1024)
        openfile(sock, 3, 'events', 0)
        pending(server, sock)
        # These replies must make progress while the event Tread is still pending.
        rpc(sock, 124, struct.pack('<I', 1), tag=80)
        rpc(sock, 108, struct.pack('<H', 600), tag=81)
        if mode == 'normal':
            emit(server, 'ee')
            record(sock, 50, b'one')
            pending(server, sock, 51)
            record(sock, 51, b'two')
            emit(server)
            pending(server, sock, 52, 1)
            receive(sock, 107, 52)
            pending(server, sock, 53)
            record(sock, 53, b'one')
        elif mode == 'flush':
            rpc(sock, 108, struct.pack('<H', 50), tag=82)
            emit(server)
            pending(server, sock)
            record(sock, 50, b'one')
        elif mode == 'reset':
            rpc(sock, 100, struct.pack('<I', 1024)+string('9P2000'), tag=65535)
            rpc(sock, 104, struct.pack('<II', 1, 0xffffffff)+string('alice')+string(''), tag=83)
            openfile(sock, 3, 'events', 0)
            emit(server)
            pending(server, sock)
            record(sock, 50, b'one')
        elif mode == 'clunk':
            send(sock, 120, 82, struct.pack('<I', 3))
            receive(sock, 107, 50)
            receive(sock, 121, 82)
            openfile(sock, 3, 'events', 0)
            emit(server)
            pending(server, sock)
            record(sock, 50, b'one')
        elif mode == 'disconnect':
            sock.close()
            sock = connection(port, 'alice', 1024)
            openfile(sock, 3, 'events', 0)
            emit(server)
            pending(server, sock)
            record(sock, 50, b'one')
        elif mode == 'duptag':
            send(sock, 108, 50, struct.pack('<H', 600))
            if sock.recv(1) != b'':
                raise RuntimeError('duplicate outstanding tag was answered')
        elif mode != 'end':
            raise RuntimeError('unknown fixture mode')
        output, failure = server.communicate(input='s', timeout=5)
        if server.returncode or 'PASS original C deferred read TCP:' not in output:
            raise RuntimeError(f'server {server.returncode}: {output} {failure}')
        print(f'PASS real original C deferred reads: {mode}')
    finally:
        if sock is not None:
            sock.close()
        if server.poll() is None:
            server.terminate()
            server.communicate(timeout=5)


if __name__ == '__main__':
    main()
