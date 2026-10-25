/* Compare actual retained decimal conversion and formatting with independent binary64 bits. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern double native_test_strtod(const char *, char **); /* Retained parser renamed by the fixture driver. */
extern char *g_fmt(char *, double, int);                 /* Actual retained shortest-decimal formatter. */


/* Read decimal/expected-bit pairs from stdin and check parser plus formatter round trips.
 * No arguments or owned resources escape; return nonzero with the failing input on mismatch. */
int main(void)
{
    char source[256];
    char formatted[64];
    char *end;
    unsigned long long expected;
    uint64_t bits;
    double value;
    unsigned long count = 0;
    if (sizeof(value) != sizeof(bits))
    {
        fprintf(stderr, "binary64 fixture requires an eight-byte double\n");
        return 1;
    }
    while (scanf("%255s %llx", source, &expected) == 2)
    {
        value = native_test_strtod(source, &end);
        memcpy(&bits, &value, sizeof(bits));
        if (*end || bits != (uint64_t)expected)
        {
            fprintf(stderr,
                    "decimal parser mismatch for %s: %llx instead of %llx\n",
                    source,
                    (unsigned long long)bits,
                    expected);
            return 1;
        }
        g_fmt(formatted, value, 'e');
        value = native_test_strtod(formatted, &end);
        memcpy(&bits, &value, sizeof(bits));
        if (*end || bits != (uint64_t)expected)
        {
            fprintf(stderr, "decimal formatter mismatch for %s via %s\n", source, formatted);
            return 1;
        }
        count++;
    }
    if (!count || ferror(stdin))
    {
        fprintf(stderr, "decimal fixture received no complete independent cases\n");
        return 1;
    }
    printf("PASS native dtoa parser/formatter: %lu independent binary64 cases\n", count);
    return 0;
}
