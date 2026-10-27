implement RegexUnicode;

include "sys.m";
include "regex.m";

RegexUnicode: module
{
    init: fn(); # Parameterless hosted regression entry.
};

sys: Sys; # Real diagnostic output supplied by the selected VM host.
regex: Regex; # Actual separately compiled library; no substitute matcher.
checks: int; # Successful literal, capture and character-class checks.


# Check one compiled expression against expected rune offsets and capture count.
# Inputs are borrowed strings/scalars. Return false after a diagnostic on failure;
# the caller stops before attempting later patterns. No owner escapes the call.
match(pattern, text: string, count, begin, end: int): int
{
    (compiled, error) := regex->compile(pattern, 1);
    if(compiled == nil) {
        sys->print("FAIL regex compile: %s\n", error);
        return 0;
    }
    result := regex->execute(compiled, text);
    if(len result != count) {
        sys->print("FAIL regex capture count: %d != %d\n", len result, count);
        return 0;
    }
    for(index := 0; index < count; index++) {
        (first, last) := result[index];
        if(first != begin || last != end) {
            sys->print("FAIL regex rune offsets: %d:%d != %d:%d\n", first, last, begin, end);
            return 0;
        }
    }
    checks++;
    return 1;
}


# Cover the old tag/sentinel collision range and Unicode boundary representatives.
# Every pattern is compiled by Regex itself and executed on the selected VM.
# Failures stop with FAIL output; successful completion reports the exact count.
init()
{
    sys = load Sys Sys->PATH;
    regex = load Regex "regex.dis";
    if(regex == nil) {
        sys->print("FAIL load regex: %r\n");
        return;
    }
    runes := array[] of {16r61, 16r3a9, 16rffff, 16r10000, 16r10001,
        16r10002, 16r10003, 16r10004, 16r10005, 16r10006, 16r10007,
        16r10008, 16r10009, 16r1000a, 16r1000b, 16r1f600, 16r10ffff};
    for(index := 0; index < len runes; index++) {
        rune := " ";
        rune[0] = runes[index];
        if(!match(rune, rune, 1, 0, 1)) {
            return;
        }
        if(!match("("+rune+"+)", "!"+rune+rune+"?", 2, 1, 3)) {
            return;
        }
        if(!match("["+rune+"]", rune, 1, 0, 1)) {
            return;
        }
        if(!match("\\"+rune, rune, 1, 0, 1)) {
            return;
        }
        if(!match("("+rune+"|z)", rune, 2, 0, 1)) {
            return;
        }
    }
    (bad, error) := regex->compile("(", 1);
    if(bad != nil || error == nil) {
        sys->print("FAIL malformed regex accepted\n");
        return;
    }
    sys->print("PASS regex Unicode: %d rune/capture/class checks\n", checks);
}
