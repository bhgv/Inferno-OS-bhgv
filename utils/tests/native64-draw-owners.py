#!/usr/bin/env python3
"""Execute real Heap/Type wrapper ownership; optionally check fresh attach pairs and dirty Image headers."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Keep actual heap/refcount/type/protected-allocation code; native allocation/I-O stay bounded services.
def unit(control=None, attach=False):
    support=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    fn=support['function']
    prefix=(ROOT/'libinterp/tests/dis64-runtime.c').read_text().split('IMPLEMENTATION')[0]
    prefix='#include <stdarg.h>\n'+prefix
    prefix=prefix.replace('Scalar objects have no marked fields; a collector traversal is outside this fixture.', 'Collector traversal is not executed; real mapped-reference retirement is checked separately.',1)
    prefix=prefix.replace('static void\nrequire(', 'static void trackedfree(void*); // Control-stop ledger retirement.\n\nstatic void\nrequire(',1)
    prefix=prefix.replace('abort();','for(int i=0;i<RuntimeBlocks;i++) if(blocks[i].pointer!=NULL) trackedfree(blocks[i].pointer);\n\t\texit(10);',1)
    prefix=prefix.replace('Dis64 runtime check failed','Draw owner semantic mismatch',1)
    prefix=prefix.replace('Type Tmodlink = {1, freemodlink, markheap, -1, 1, 0, 0, {0x80}};','Type Tmodlink = {1}; // Unused module descriptor; no module execution is modeled.',1)
    prefix=prefix.replace('void\npoolfree(', 'static void resourcepoolobserve(void*); // Observe exact result-publication ordering.\n\nvoid\npoolfree(',1)
    prefix=prefix.replace('trackedfree((uchar*)pointer-O(Bhdr, u.data));','resourcepoolobserve(pointer);\n\ttrackedfree((uchar*)pointer-O(Bhdr, u.data));',1)
    old=fn(prefix,'void\nkwerrstr(')
    prefix=prefix.replace(old,'''void
kwerrstr(char *format,...)
{
    va_list args;
    va_start(args,format);
    vsnprintf(message,sizeof(message),format,args);
    va_end(args);
}''',1)
    heap=(ROOT/'libinterp/heap.c').read_text()
    parts=[fn((ROOT/'emu/port/dis.c').read_text(),'int\nkprotect(')]
    parts += [fn(heap,sig) for sig in ('void\nfreeptrs(','void\nfreeheap(','void\ndestroy(','Type*\ndtype(','void\nfreetype(','void\ninitmem(','Heap*\nheapz(','Heap*\nheap(','void\nfreestring(')]
    parts[3]=parts[3].replace('void\ndestroy(','void\nactualdestroy(',1)
    draw=(ROOT/'libinterp/draw.c').read_text()
    declarations='''#include "draw.h"
#include "draw-native64.h"
#include "nativeptrmap.h"
#include <stdarg.h>
typedef struct DRef DRef;
typedef struct DDisplay DDisplay;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct DFont DFont;
'''
    declarations+=draw[draw.index('struct DRef\n{'):draw.index('Cache*\tsfcache')]
    declarations+='static Cache *fcache[1]; // Bounded cache-install service, no cache algorithm claim.\nchar deffontname[]=\"*default*\"; // Existing built-in font identity.\n'
    declarations+=draw[draw.index('uchar fontmap[]'):draw.index('Draw_Image*\tallocdrawimage')]
    declarations+=fn(draw,'static int\ndrawmaketypes(')+'\n\n'
    declarations+='#ifdef DIS_NATIVE64_DRAW_HEADER\n'+fn(draw,'void\ndrawmodinit(')+'\n#endif\n'
    declarations+=draw[draw.index('typedef struct DrawHeapBuild'):draw.index('/* Hash bytes with defined unsigned wraparound')]
    names=('display_dec','freedrawdisplay','freedrawscreen','freedrawfont','freedrawimage','lookupimage','lookupscreen','lookupfont','lookupdisplay','checkimage','checkscreen','checkdisplay','mkdrawimage','allocdrawimage','allocdrawscreen','Display_newimage','Display_colormix','Screen_allocate','Display_publicscreen','Screen_newwindow','drawownedfont','Font_open','Font_build','drawownedimage','Display_rgb','Display_open','Display_namedimage','Display_readimage','color','Display_color','Image_name','Display_writeimage','Image_origin','screentopbot','Screen_top','Screen_bottom','Image_top','Image_bottom','Image_flush')
    if attach:
        names=('display_dec','freedrawdisplay','freedrawscreen','freedrawfont','freedrawimage','lookupimage','lookupscreen','lookupfont','lookupdisplay','checkimage','checkscreen','checkdisplay','mkdrawimage','allocdrawimage','allocdrawscreen','mkdrawscreen','Display_getwindow','Display_startrefresh')
    signatures={n:('Image*' if n=='lookupimage' else 'Screen*' if n=='lookupscreen' else 'Font*' if n=='lookupfont' else 'Display*' if n=='lookupdisplay' else 'Image*' if n=='checkimage' else 'Screen*' if n=='checkscreen' else 'Display*' if n=='checkdisplay' else 'Draw_Image*' if n in ('mkdrawimage','allocdrawimage') else 'Draw_Screen*' if n=='mkdrawscreen' else 'static Draw_Image*' if n=='drawownedimage' else 'Draw_Image*' if n=='color' else 'static Draw_Font*' if n=='drawownedfont' else 'static void' if n=='screentopbot' else 'static DScreen*' if n=='allocdrawscreen' else 'void')+'\n'+n+'(' for n in names}
    bodies='\n\n'.join(fn(draw,signatures[n]) for n in names)
    for name in ('Display_newimage','Display_colormix','Screen_allocate','Display_publicscreen','Screen_newwindow','Font_open','Font_build','Display_rgb','Display_open','Display_namedimage','Display_readimage','Display_color','Image_name','Display_writeimage','Image_origin','Screen_top','Screen_bottom','Image_top','Image_bottom','Image_flush','Display_getwindow','Display_startrefresh'):
        bodies=bodies.replace('F_'+name,'F_Draw_'+name).replace('\n'+name+'(', '\nDraw_'+name+'(')
    if not attach:
        vector=fn(bodies,'static void\nscreentopbot(')
        bodies=bodies.replace(vector,vector.replace('malloc(', 'vectorallocate(').replace('free(', 'trackedfree('),1)
    native='static int screenid; // Private native protocol counter from window.c.\n'
    native+='\n\n'.join(fn((ROOT/'libdraw/window.c').read_text(),sig) for sig in ('Screen*\nallocscreen(','Screen*\npublicscreen(','int\nfreescreen('))
    native='#define malloc(n) nativeallocate(n,2) // Actual native Screen allocations.\n#define free(p) nativeretire(p,2) // Observe local native retirement.\n'+native+'\n#undef malloc\n#undef free\n'
    native+='\n\n'+fn((ROOT/'libdraw/alloc.c').read_text(),'int\n_freeimage1(')
    native+='\n#define free(p) nativeretire(p,1) // Native Image ownership, independent of managed Heap slots.\n'+fn((ROOT/'libdraw/alloc.c').read_text(),'int\nfreeimage(')+'\n#undef free\n'
    native+='\n\n'+'\n\n'.join(fn((ROOT/'libdraw/window.c').read_text(),sig) for sig in ('static void\ntopbottom(','void\ntopwindow(','void\nbottomwindow(','void\ntopnwindows(','void\nbottomnwindows(','static int\nwindoworiginrect(','int\noriginwindow('))
    if attach:
        native+='\n\n'+'\n\n'.join(fn((ROOT/'libdraw/init.c').read_text(),sig) for sig in ('static void\nwindowdiscard(','int\ngengetwindow('))
    if attach:
        initializers=(ROOT/'libdraw/drawinfo.h').read_text()+'\n\n'+'\n\n'.join(fn((ROOT/'libdraw/alloc.c').read_text(),sig) for sig in ('Image*\n_allocimage(', 'Image*\nnamedimage('))
        initializers=initializers.replace('_allocimage(', 'freshallocimage(').replace('namedimage(', 'freshnamedimage(')
        if control=='image-callback': initializers=initializers.replace('i->reffn = nil;', '/* missing callback initialization */')
        if control=='info-range': initializers=initializers.replace('if(value > (limit-', 'if(0 && value > (limit-',1)
        if control=='info-publication': initializers=initializers.replace('int fields[12], index, start, end;', 'int fields[12], index, start, end; image->depth=0;',1)
        if control=='initial-flush': initializers=initializers.replace('if(flushimage(d, 0) < 0)\n\t\tgoto Error;', 'flushimage(d,0);',1)
        if control=='info-release': initializers=initializers.replace("a[0] = 'f';", "a[0] = 'x';")
        if control=='image-path': initializers=initializers.replace('i->fpath = nil;', '/* missing path initialization */')
        native+='\n#define malloc(n) poisonedimage(n) // Poison fresh headers to expose omitted field initialization.\n#define free(p) nativeretire(p,1) // Retire exact native header owners.\n'+initializers+'\n#undef malloc\n#undef free\n'
    bodies=bodies.replace('free(dr);','nativeretire(dr,5);',1)
    if control=='mapped':
        actual=fn(bodies,'void\nfreedrawimage('); bodies=bodies.replace(actual,actual.replace('freeptrs(di, h->t);','USED(di);',1),1)
    if control=='double':
        actual=fn(bodies,'void\nfreedrawscreen(');bodies=bodies.replace(actual,actual.replace('freeptrs(ds, h->t);','{ freeptrs(ds, h->t); freeptrs(ds, h->t); }',1),1)
    if control=='rollback':
        actual=fn(bodies,'Draw_Image*\nallocdrawimage(');bodies=bodies.replace(actual,actual.replace('freeimage(image);','USED(image);',1),1)
    if control=='native':native=native.replace('free(s);\n\treturn result;', 'if(result >= 0) free(s);\n\treturn result;',1)
    if control=='alias':
        actual=fn(bodies,'void\nDraw_Display_newimage(');bodies=bodies.replace(actual,actual.replace('source = f->d;', 'destroy(old); old=H;\n\tsource = f->d;',1),1)
    if control=='swept':
        actual=fn(bodies,'void\nfreedrawimage(');bodies=bodies.replace(actual,actual.replace('if(!swept)','if(1)',1),1)
    if control=='lookup':
        actual=fn(bodies,'void\nfreedrawimage(');bodies=bodies.replace(actual,actual.replace('image = di->image;','image = lookupimage(&di->drawimage);',1),1)
    if control=='detach':
        actual=fn(bodies,'void\nfreedrawimage(');bodies=bodies.replace(actual,actual.replace('di->image = nil;','/* omitted native detachment */',1),1)
    if control=='font-rollback':
        actual=fn(bodies,'static Draw_Font*\ndrawownedfont(');bodies=bodies.replace(actual,actual.replace('font_close(font);','USED(font);',1),1)
    if control=='font-name':
        actual=fn(bodies,'static Draw_Font*\ndrawownedfont(');bodies=bodies.replace(actual,actual.replace('D2H(name)->ref++;','USED(name);',1),1)
    if control=='loaded-rollback':
        actual=fn(bodies,'static Draw_Image*\ndrawownedimage(');bodies=bodies.replace(actual,actual.replace('drawdiscardimage(image);','USED(image);',1),1)
    if control=='loaded-name':
        actual=fn(bodies,'void\nDraw_Display_namedimage(');bodies=bodies.replace(actual,actual.replace('D2H(name)->ref++;','USED(name);',1),1)
    if control=='loaded-alias':
        actual=fn(bodies,'void\nDraw_Display_readimage(');bodies=bodies.replace(actual,actual.replace('source = f->d;', 'destroy(old); old=H;\n\tsource = f->d;',1),1)
    if control=='name-retain':
        actual=fn(bodies,'void\nDraw_Image_name(');bodies=bodies.replace(actual,actual.replace('D2H(replacement)->ref++;','USED(replacement);',1),1)
    if control=='name-result':
        actual=fn(bodies,'void\nDraw_Image_name(');bodies=bodies.replace(actual,actual.replace('\tf = fp;', '\tf = fp;\n\tresult=-1; memmove(f->ret,&result,sizeof(result));',1),1)
    if control=='origin-status':
        actual=fn(native,'int\noriginwindow(');native=native.replace(actual,actual.replace('if(b == nil)\n\t\treturn -1;', 'if(b == nil)\n\t\treturn 1;',1),1)
    if control=='origin-wire':
        actual=fn(native,'int\noriginwindow(');native=native.replace(actual,actual.replace('BPLONG(b+5, log.x);','BPLONG(b+5, log.y);',1),1)
    if control=='order-wire':
        actual=fn(native,'static void\ntopbottom(');native=native.replace(actual,actual.replace('b[1] = top;', 'b[1] = !top;',1),1)
    native+='\n\n'+'\n\n'.join(re.sub(r'^#include[^\n]*\n','',(ROOT/p).read_text(),flags=re.M) for p in ('libdraw/arith.c','libdraw/chan.c'))
    if control=='window':
        start=native.index('\tif(i->screen){',native.index('int\n_freeimage1('));end=native.index('\ti->next = nil;',start)
        native=native[:start]+native[end:]
    if control=='retry':native=native.replace('\tif(try == 25){\n\t\tfree(s);\n\t\treturn nil;\n\t}', '',1)
    if control=='attach-fd': native=native.replace('libclose(fd);','USED(fd);',1)
    if control=='attach-publish': native=native.replace('*winp = window;\n\t*scrp = screen;\n\twindowdiscard(d, oldwindow, oldscreen);','windowdiscard(d, oldwindow, oldscreen);\n\t*winp = window;\n\t*scrp = screen;',1)
    if control=='screen-fill':
        actual=fn(bodies,'Draw_Screen*\nmkdrawscreen(');bodies=bodies.replace(actual,actual.replace('D2H(fill)->ref++;','USED(fill);',1),1)
    if control=='screen-backing':
        actual=fn(bodies,'Draw_Screen*\nmkdrawscreen(');bodies=bodies.replace(actual,actual.replace('result->drawscreen.image = backing;','result->drawscreen.image = backing; D2H(backing)->ref++;',1),1)
    if control=='attach-rollback':
        actual=fn(bodies,'void\nDraw_Display_getwindow(');bodies=bodies.replace(actual,actual.replace('freeimage(native);','USED(native);'),1)
    if control=='attach-tuple':
        actual=fn(bodies,'void\nDraw_Display_getwindow(');bodies=bodies.replace(actual,actual.replace('memmove(slot, result, sizeof(result));\n\tdestroy(old[0]);\n\tdestroy(old[1]);','destroy(old[0]);\n\tdestroy(old[1]);\n\tmemmove(slot, result, sizeof(result));',1),1)
    fixture=(ROOT/'utils/tests/native64-draw-owners.c').read_text()
    if attach:
        fixture=fixture.replace('NATIVE_IMPLEMENTATION', 'ATTACH_SERVICES\nNATIVE_IMPLEMENTATION')
        fixture=fixture.replace('static uchar wire[64];', 'static int attachmode, filemode, fileopens, filecloses, refreshcalls; // Bounded attach service observations.\nstatic void **rawslot; // Publication observer for exclusively owned raw pairs.\nstatic Image *rawold; // Raw pair identity retained until publication.\nstatic void *tuplewatch; // Managed old image observed before native retirement.\nstatic void *tupleold; // Exact displaced managed image identity.\nstatic void *tupleslot; // Possibly unaligned two-owner result slot.\nstatic uchar wire[64];')
        fixture=fixture.replace('image=pointer;', 'image=pointer;\n\t\tif(pointer==rawold && rawslot!=nil) require(rawslot[0]!=rawold);\n\t\tif(pointer==tuplewatch){ void *pair[2]; memmove(pair,tupleslot,sizeof(pair)); require(pair[1]!=tupleold); }',1)
        fixture=fixture.replace('screen=pointer;', 'screen=pointer;\n\t\tfor(cursor=screen->display->windows;cursor!=nil;cursor=cursor->next) require(cursor->screen!=screen);',1)
        fixture=fixture.replace('Rect(2,3,9,11), RGBA32, 1, 0', 'attachmode ? Rect(0,0,100,80) : Rect(2,3,9,11), RGBA32, 1, 0')
        services, operations=(ROOT/'utils/tests/native64-draw-attach.c').read_text().split('ATTACH_CASES')
        operations=runpy.run_path(str(ROOT/'utils/tests/native64-draw-info.py'))['rows']()+operations
        fixture=fixture.replace('ATTACH_SERVICES', services)
        fixture=fixture.replace('WINDOW_OPERATIONS', operations)
        fixture=fixture.replace(fn(fixture,'int\nmain('), fn(operations,'int\nattachmain(').replace('attachmain(', 'main(',1))
        declarations+='Draw_Screen *mkdrawscreen(Screen*, Draw_Display*); // Actual private attach wrapper.\n'

    fixture=fixture.replace('WINDOW_OPERATIONS',(ROOT/'utils/tests/native64-draw-owners-ops.c').read_text())
    fixture=fixture.replace('DRAW_DECLARATIONS',declarations).replace('HEAP_IMPLEMENTATION','\n\n'.join(parts)).replace('NATIVE_IMPLEMENTATION',native).replace('DRAW_IMPLEMENTATION',bodies)
    return prefix+'\n\n'+fixture


# Native target compilation and semantic controls never treat a signal/sanitizer/compiler failure as success.
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',type=Path,required=True);parser.add_argument('--attach-only',action='store_true');args=parser.parse_args()
    env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
    command=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))+['-ffunction-sections','-fdata-sections']
    if arch=='arm64':command+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
    elif arch=='riscv64':command+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
    runner=shlex.split(env.get('NATIVE64_RUNNER',''))
    support=runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    with tempfile.TemporaryDirectory(prefix='native64-draw-owners-',dir=env.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);env['TMPDIR']=str(work)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()),'-H','-M','Draw','-I'+str(ROOT/'module'),str(ROOT/'module/draw-native64.m')],env=env))
        includes=['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        modes=('WORD32','WORD32-file','attach-fd','attach-publish','screen-fill','screen-backing','attach-rollback','attach-tuple','image-callback','image-path','info-range','info-publication','initial-flush','info-release') if args.attach_only else ('gated','WORD32','WORD32-file','mapped','double','rollback','native','alias','swept','lookup','detach','window','retry','font-rollback','font-name','loaded-rollback','loaded-name','loaded-alias','name-retain','name-result','origin-status','origin-wire','order-wire')
        for mode in modes:
            source=work/(mode+'.c');source.write_text(support['portfixture'](unit(None if mode.startswith('WORD') or mode=='gated' else mode,attach=args.attach_only),'#include "vmfetch.h"'))
            binary=work/mode;flags=['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode.endswith('file') else [])+(['-DDIS_NATIVE64_DRAW_HEADER=1'] if mode=='gated' else [])
            subprocess.run(command+flags+includes+[str(source),'-Wl,--gc-sections','-Wl,--wrap=free','-lm','-o',str(binary)],env=env,check=True)
            result=subprocess.run(runner+[str(binary)],env=env,capture_output=True,text=True,timeout=90)
            if mode.startswith('WORD') or mode=='gated':
                if result.returncode:raise RuntimeError(result.stdout+result.stderr)
                print(mode+': '+result.stdout,end='',flush=True)
            elif result.returncode!=10 or 'Draw owner semantic mismatch' not in result.stderr:raise RuntimeError('Owner control missed predicate '+mode+' '+result.stdout+result.stderr)
            else:print('PASS Draw owner semantic control:',mode,arch,flush=True)


if __name__=='__main__':main()
