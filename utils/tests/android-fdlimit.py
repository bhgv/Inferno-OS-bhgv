#!/usr/bin/env python3
"""Execute the Android helper against bounded native resource-limit interfaces."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
SOURCE=r'''
#include <sys/resource.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include "fdlimit.h"
static int mode, fallbacks;


/* Supply independently chosen finite, overflowing, unlimited and unavailable descriptor limits. */
int __wrap_getrlimit(int resource, struct rlimit *limit)
{
    if(resource!=RLIMIT_NOFILE) return -1;
    limit->rlim_cur=1024;
    if(mode==1) limit->rlim_cur=(rlim_t)INT_MAX+1;
    if(mode==2) limit->rlim_cur=RLIM_INFINITY;
    if(mode==3 || mode==4) return -1;
    if(mode==5) limit->rlim_cur=0;
    if(mode==6) limit->rlim_cur=INT_MAX;
    return 0;
}


/* Preserve explicit sysconf failure and exercise a native long wider than the caller's int. */
long __wrap_sysconf(int selector)
{
    fallbacks++;
    if(selector!=_SC_OPEN_MAX) return -1;
    if(mode==2) return (long)INT_MAX+1;
    return mode==4 ? -1 : 4096;
}


/* Keep every result and fallback-selection predicate active under NDEBUG. */
int main(void)
{
    int expected[]={1024,INT_MAX,INT_MAX,4096,-1,0,INT_MAX};
    for(mode=0;mode<7;mode++){
        fallbacks=0;
        if(inferno_android_fdlimit()!=expected[mode]) return 10+mode;
        if(fallbacks!=((mode>=2 && mode<=4)?1:0)) return 20+mode;
    }
    puts("PASS Android descriptor limit: finite/zero/exact/overflow/unlimited/fallback/error cases");
    return 0;
}
'''


# libc resources are bounded adapters; the production helper and its numeric conversions execute unchanged.
def main():
    with tempfile.TemporaryDirectory(prefix='android-fdlimit-',dir=ROOT) as directory:
        work=Path(directory)
        (work/'test.c').write_text(SOURCE)
        subprocess.run(['cc','-O3','-DNDEBUG','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'emu/Android'),str(ROOT/'emu/Android/getdtablesize.c'),str(work/'test.c'),'-Wl,--wrap=getrlimit','-Wl,--wrap=sysconf','-o',str(work/'run')],check=True)
        subprocess.run([str(work/'run')],check=True)


if __name__=='__main__':
    main()
