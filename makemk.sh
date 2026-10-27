#!/bin/sh
# Bootstrap a native mk without evaluating target mk syntax as shell code.
set -eu

ROOT=${INFERNO_ROOT:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}
SYSTARG=${MKHOSTOS:-$(uname -s)}
case ${MKHOSTARCH:-$(uname -m)} in
    x86_64|amd64) hostarch=amd64 ;;
    aarch64|arm64) hostarch=arm64 ;;
    i?86|386) hostarch=386 ;;
    arm|armv*) hostarch=arm ;;
    *) echo "unsupported bootstrap host architecture" >&2; exit 1 ;;
esac
case $SYSTARG in
    Linux) headers=$ROOT/Linux/386/include ;;
    *) echo "native bootstrap currently supports Linux hosts" >&2; exit 1 ;;
esac

CC=${CC:-cc}
AR=${AR:-ar}
CFLAGS=${CFLAGS:--O2 -g -fno-strict-aliasing}
LDFLAGS=${LDFLAGS:-}
bindir=${MKBOOTSTRAP_BINDIR:-$ROOT/$SYSTARG/$hostarch/bin}
builddir=$(mktemp -d "$ROOT/.bootstrap-mk.XXXXXX")


# Remove only the private directory created by this invocation.
cleanup()
{
    find "$builddir" -depth -delete
    if test -d "$builddir"; then
        echo "bootstrap cleanup failed: $builddir" >&2
        return 1
    fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM


# Compile a group into an isolated archive, retaining ordinary CC/CFLAGS overrides.
buildlib()
{
    name=$1
    shift
    directory=$builddir/$name
    mkdir -p "$directory"
    for source in "$@"; do
        object=$directory/$(basename "$source" .c).o
        if test ! -f "$object"; then
            "$CC" $CFLAGS -c -I"$headers" -I"$ROOT/include" -I"$ROOT/utils/include" "$source" -o "$object"
        fi
    done
    "$AR" cr "$builddir/lib$name.a" "$directory"/*.o
}

# Check compiler identity before a mislabeled cross binary can replace native tools.
cat > "$builddir/host.c" <<'HOST'
#if !defined(__linux__)
#error bootstrap requires a Linux compiler
#endif
#if defined(MK_HOST_amd64) && (!defined(__x86_64__) || __SIZEOF_POINTER__ != 8)
#error compiler does not target Linux amd64
#endif
#if defined(MK_HOST_arm64) && (!defined(__aarch64__) || __SIZEOF_POINTER__ != 8)
#error compiler does not target Linux arm64
#endif
#if defined(MK_HOST_386) && (!defined(__i386__) || __SIZEOF_POINTER__ != 4)
#error compiler does not target Linux 386
#endif
#if defined(MK_HOST_arm) && (!defined(__arm__) || __SIZEOF_POINTER__ != 4)
#error compiler does not target Linux arm
#endif
int bootstrap_host;
HOST
"$CC" $CFLAGS "-DMK_HOST_$hostarch" -c "$builddir/host.c" -o "$builddir/host.o"

# These POSIX declarations describe build tools, not the selected VM target ABI.
# No mkconfig sourcing, target compiler, target assembly or existing output cleanup.
buildlib regexp "$ROOT/utils/libregexp/regaux.c" "$ROOT/utils/libregexp/regcomp.c" \
    "$ROOT/utils/libregexp/regerror.c" "$ROOT/utils/libregexp/regexec.c" \
    "$ROOT/utils/libregexp/regsub.c" "$ROOT/utils/libregexp/rregexec.c" \
    "$ROOT/utils/libregexp/rregsub.c"
buildlib bio "$ROOT"/libbio/*.c
buildlib 9 \
    "$ROOT/lib9/dirstat-posix.c" "$ROOT/lib9/rerrstr.c" \
    "$ROOT/lib9/errstr-posix.c" "$ROOT/lib9/getuser-posix.c" \
    "$ROOT/lib9/argv0.c" "$ROOT/lib9/charstod.c" "$ROOT/lib9/cleanname.c" "$ROOT/lib9/create.c" \
    "$ROOT/lib9/dirwstat.c" "$ROOT"/lib9/*print*.c "$ROOT"/lib9/*fmt*.c \
    "$ROOT/lib9/exits.c" "$ROOT/lib9/getfields.c" "$ROOT/lib9/pow10.c" \
    "$ROOT/lib9/qsort.c" "$ROOT/lib9/rune.c" "$ROOT/lib9/runestrlen.c" \
    "$ROOT/lib9/seek.c" "$ROOT/lib9/strdup.c" "$ROOT/lib9/strtoll.c" \
    "$ROOT/lib9/utflen.c" "$ROOT/lib9/utfrrune.c" "$ROOT/lib9/utfrune.c" \
    "$ROOT"/lib9/utf*.c "$ROOT"/lib9/*str*cpy*.c

mkdir -p "$builddir/mk"
for source in Posix sh arc archive bufblock env file graph job lex main match mk parse recipe rule run shprint symtab var varsub word; do
    "$CC" $CFLAGS -c -I"$headers" -I"$ROOT/include" -I"$ROOT/utils/include" "$ROOT/utils/mk/$source.c" -o "$builddir/mk/$source.o"
done
"$CC" $CFLAGS $LDFLAGS -o "$builddir/mk.native" "$builddir"/mk/*.o \
    "$builddir/libregexp.a" "$builddir/libbio.a" "$builddir/lib9.a"
# The grammar generator uses the same native archives; target code is not linked.
"$CC" $CFLAGS -I"$headers" -I"$ROOT/include" -I"$ROOT/utils/include" \
    '-DROOT="'"$ROOT"'"' \
    "$ROOT/utils/iyacc/yacc.c" "$builddir/libbio.a" "$builddir/lib9.a" \
    $LDFLAGS -o "$builddir/iyacc.native"
mkdir -p "$bindir"
# Rename complete binaries so compilation/link failures preserve previous tools.
for tool in mk iyacc; do
    cp "$builddir/$tool.native" "$bindir/$tool.new.$$"
    mv "$bindir/$tool.new.$$" "$bindir/$tool"
done
cleanup
trap - EXIT HUP INT TERM
printf 'native tools installed: %s (mk, iyacc; %s/%s)\n' "$bindir" "$SYSTARG" "$hostarch"
