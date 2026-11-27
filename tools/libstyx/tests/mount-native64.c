/* Copyright (c) 2026 bhgv
 * EN: Test-only boot probe of the real Inferno mount client and live Styx server.
 * RU: Тестовый boot-probe настоящего mount-клиента Inferno и живого сервера Styx.
 * Included only by a separate acceptance build after normal kernel headers.
 */

/* EN/RU: Require an operation and retain its kernel diagnostic.
 * Проверить операцию с сохранением сообщения ядра. */
static void
mountcheck(int valid, char *name)
{
    print("mount checkpoint %s\n", name);
    if(!valid)
        panic("native64 mount acceptance %s: %r", name);
}

/* EN/RU: Dial address, mount the fixture, check live fids and native cleanup.
 * Подключиться к адресу, смонтировать fixture, проверить fid и освобождение.
 * Runs on the owning kernel process before guest scheduling; no foreign-thread calls.
 * Выполняется в процессе ядра до guest scheduler, не из чужого потока. */
static void
mountacceptance(char *address)
{
    int connection, control, file, directory, n;
    char buffer[1024], version[32], strings[1024];
    Dir info;

    setid("alice", 0);
    connection = kdial(address, nil, nil, nil);
    mountcheck(connection >= 0, "dial");
    strcpy(version, "9P2000");
    mountcheck(kfversion(connection, 1024, version, sizeof(version)) == 6, "version");
    mountcheck(kmount(connection, -1, "/mnt", MREPL, "") >= 0, "mount");
    mountcheck(kclose(connection) == 0, "release original transport fd");
    directory = kopen("/mnt", OREAD);
    file = kopen("/mnt/first", OREAD);
    control = kopen("/mnt/ctl", OWRITE);
    mountcheck(directory >= 0 && file >= 0 && control >= 0, "open fids");
    n = kread(file, buffer, sizeof(buffer));
    mountcheck(n == 7 && memcmp(buffer, "initial", 7) == 0, "initial file");
    mountcheck(kseek(file, 0x123456789abcdefLL, 0) == 0x123456789abcdefLL, "64-bit seek");
    mountcheck(kread(file, buffer, sizeof(buffer)) == 0, "64-bit offset EOF");
    mountcheck(kstat("/mnt/first", (uchar*)buffer, sizeof(buffer)) == 0, "stat status");
    n = GBIT16((uchar*)buffer) + BIT16SZ;
    mountcheck(n > 0 && convM2D((uchar*)buffer, n, &info, strings) == n &&
        info.qid.path == 0xfedcba9876543210ULL, "full-width Qid stat");
    mountcheck(kwrite(control, "change", 6) == 6, "live change");
    mountcheck(kseek(file, 0, 0) == 0, "rewind retained file");
    n = kread(file, buffer, sizeof(buffer));
    mountcheck(n == 7 && memcmp(buffer, "changed", 7) == 0, "retained file contents");
    n = kread(directory, buffer, sizeof(buffer));
    mountcheck(n > 0 && convM2D((uchar*)buffer, n, &info, strings) == n &&
        strcmp(info.name, "later") == 0 && info.mode == 0640, "live directory");
    mountcheck(kread(directory, buffer, sizeof(buffer)) == 0, "directory EOF");
    mountcheck(kclose(directory) == 0 && kclose(file) == 0 && kclose(control) == 0, "close fids");
    mountcheck(kunmount(nil, "/mnt") == 0, "unmount");
    print("PASS native64 kernel Styx mount: live fids, Qid64, offset64, read/write/stat, unmount\n");
    cleanexit(0);
}
