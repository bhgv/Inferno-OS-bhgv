"""Attach the production collector and sweepable arena to the client execution gate."""
from pathlib import Path
import runpy
ROOT=Path(__file__).resolve().parents[2]
def instrument(source,control=None):
 extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
 for signature in ('void\nmarkheap(', 'void*\npoolalloc(', 'void\npoolfree('):source=source.replace(extract(source,signature),'',1)
 source=source.replace('int mutator, nprop, gchalt;','')
 source=source.replace(extract(source,'int hmsize('),'int hmsize(void *h){Bhdr *b=(Bhdr*)((uchar*)h-O(Bhdr,u.data));require(b->magic==MAGIC_A);return b->size-O(Bhdr,u.data);}',1)
 collector='\n'.join(l for l in (ROOT/'libinterp/gc.c').read_text().splitlines()if not l.startswith('#include'))
 for signature in ('void\nptradd(', 'void\nptrdel('):collector=collector.replace(extract(collector,signature),'',1)
 collector=collector.replace(extract(collector,'static void\nptrmark('),'static void ptrmark(void){for(int i=0;i<nlinkpins;i++)Setmark(linkpins[i]);}')
 arena=(ROOT/'utils/tests/dis64-numeric-gc.c').read_text().split('/* Run single-instruction quanta')[0].replace('#define GcSlots 8','#define GcSlots 256').replace('#define GcSlotBytes 256','#define GcSlotBytes 2048')
 arena=arena.replace(extract(arena,'int\nprint('),'',1)
 arena=arena.replace('require(pool == heapmem && bytes <= GcSlotBytes-O(Bhdr, u.data));','require(pool == heapmem && bytes <= GcSlotBytes-O(Bhdr, u.data));\n allocations++;if(allocations==failat)return nil;',1)
 at=source.index('static Prog *observed;');source=source[:at]+collector+'\n'+arena+'\n'+source[at:]
 source=source.replace('Type Tlist={1,freelist,markheap,sizeof(List)};','Type Tlist={1,freelist,marklist,sizeof(List)};')
 source=source.replace('Tstring.free=freeheap;','Tstring.free=freeheap;Tstring.mark=noptrs;numericpoolinit();')
 # Tbyte is also present in scalar-only fixtures. Array ownership hooks belong
 # only to the codec fixture that actually includes the production destructor.
 if 'void\nfreearray('in source:
  source=source.replace('Tstring.free=freeheap;', 'Tstring.free=freestring;Tarray.free=freearray;Tarray.mark=markarray;Tarray.size=sizeof(Array);')
  source=source.replace('Tptr.ref==1);','Tptr.ref==1 && Tarray.ref==1 && Tbyte.ref==1);')
 at=source.index('/* Both caller CODE and provider CODE');source=source[:at]+(ROOT/'utils/tests/dis64-scalar-client-gc.c').read_text()+'\n'+source[at:]
 source=source.replace('clientloads++;','clientlocalremember(ml->m,providerfile,providerfilebytes);clientloads++;')
 source=source.replace('observedispatch(m,jit);m->ss=','clientlocalremember(m,clientfile,clientfilebytes);observedispatch(m,jit);m->ss=')
 source=source.replace('clientempty(void){','clientempty(void){clientlocalcount=clientresultcount=0;for(int slot=1;slot<GcSlots;slot++)require(numericblock(slot)->magic==MAGIC_F);')
 head,tail=source.rsplit('done=kprotect(executeprocess,p);if(!done)break;',1);source=head+'done=kprotect(executeprocess,p);if(!done)break;clientcollect(p,m,root);'+tail
 if control=='frame':source=source.replace('clientproviderjit=cflag=pjit;', 'm->type[0]->mark=noptrs;clientproviderjit=cflag=pjit;')
 if control=='list':source=source.replace('clientproviderjit=cflag=pjit;', 'Tlist.mark=noptrs;clientproviderjit=cflag=pjit;')
 return source
