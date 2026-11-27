/* Copyright (c) 2026 bhgv
 * EN: Exercise linked C codecs on independent packets; never serialize native Fcall memory.
 * RU: Проверять C-кодеки независимыми пакетами; память Fcall не передаётся по сети.
 */
#include <lib9.h>
#include <fcall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* EN/RU: Refuse an invalid fixture/codec result, retaining every executed check.
 * Отклонить неверный пакет или результат кодека; проверки выполняются всегда. */
static void check(int valid, const char *reason)
{
    if (!valid) {
        fprintf(stderr, "Styx wire interop: %s\n", reason);
        exit(2);
    }
}


/* EN/RU: Read complete wire packets from argv[1], roundtrip, emit exact wire to argv[2].
 * Читать полные пакеты из argv[1], проверить кодеки и записать wire в argv[2].
 * Buffers own decoded pointer lifetimes until encoding finishes; no server/thread state.
 * Буферы живут до окончания кодирования; состояния сервера/потоков здесь нет. */
int main(int argc, char **argv)
{
    uchar wire[65536], copy[65536], output[65536];
    char strings[65536];
    FILE *input, *result;
    Fcall call;
    Dir dir;
    uint bytes, n, count = 0;

    check(argc == 3, "expected input/output files");
    input = fopen(argv[1], "rb");
    result = fopen(argv[2], "wb");
    check(input != NULL && result != NULL, "open corpus");
    while (fread(wire, 1, 4, input) == 4) {
        bytes = GBIT32(wire);
        check(bytes >= 7 && bytes <= sizeof(wire), "packet bound");
        check(fread(wire + 4, 1, bytes - 4, input) == bytes - 4, "packet body");
        for (n = 0; n < bytes; n++) {
            memcpy(copy, wire, bytes);
            check(convM2S(copy, n, &call) == 0, "truncated packet accepted");
        }
        memcpy(copy, wire, bytes);
        check(convM2S(copy, bytes, &call) == bytes, "decode");
        if (call.type == Rstat || call.type == Twstat) {
            check(statcheck(call.stat, call.nstat) == 0, "stat boundary");
            check(convM2D(call.stat, call.nstat, &dir, strings) == call.nstat, "stat decode");
            check(dir.qid.path == 0xfedcba9876543210ULL, "64-bit stat qid");
        }
        check(sizeS2M(&call) == bytes, "encoded size");
        check(convS2M(&call, output, sizeof(output)) == bytes, "encode");
        check(memcmp(wire, output, bytes) == 0, "wire changed");
        check(fwrite(output, 1, bytes, result) == bytes, "write corpus");
        count++;
    }
    check(feof(input) && !ferror(input), "read corpus");
    check(fclose(input) == 0 && fclose(result) == 0, "close corpus");
    check(count == 29, "message coverage");
    fprintf(stderr, "PASS 29 Styx vectors, all 27 legal message types and truncated prefixes\n");
    return 0;
}
