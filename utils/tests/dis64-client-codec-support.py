"""Production String/byte-array codecs for the selected client execution fixture."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def instrument(source,extract):
 strings=(ROOT/'libinterp/string.c').read_text();heap=(ROOT/'libinterp/heap.c').read_text();xec=(ROOT/'libinterp/xec.c').read_text()
 source=source.replace(extract(source,'char* string2c('),'',1)
 source=source.replace(extract(source,'void\ninitarray('),extract(heap,'void\ninitarray('),1).replace(extract(source,'Heap*\nheaparray('),extract(heap,'Heap*\nheaparray('),1)
 source=source.replace(extract(source,'String*\nc2string('),extract(strings,'String*\nc2string('),1)
 source=source.replace('REG R;','void freearray(Heap*,int);\nREG R;',1)
 support='''
#include <stdarg.h>
/* The complete-console gate uses actual lib9 formatting. Here only the two
 * standard formats used by the unmodified handlers cross this libc seam. */
int sprint(char *out,char *format,...){va_list ap;int n;va_start(ap,format);n=vsprintf(out,format,ap);va_end(ap);return n;}
ulong msize(void *pointer){for(unsigned int i=0;i<RuntimeBlocks;i++)if(blocks[i].pointer==pointer)return blocks[i].bytes;require(0);return 0;}
char exType[]="type";
'''
 support+='\n#undef T\n'+'\n'.join(l for l in(ROOT/'lib9/rune.c').read_text().splitlines()if not l.startswith('#include'))+'\n#undef Bit\n#undef T\n#undef RuneX\n#define T(r) *((void**)(R.r))\n'
 support+='\n'+extract(heap,'void\nfreestring(')+'\n'+'\n'.join(extract(heap,sig)for sig in('void\nacheck(','void\nfreearray('))
 support+='\n'+extract((ROOT/'libinterp/runt.c').read_text(),'Array*\nmem2array(')
 support+='\n#undef malloc\n#define malloc(n) trackedallocate(n,0)\n'+extract(strings,'char*\nstring2c(')+'\n#undef malloc\n#define malloc(bytes) schedmalloc(bytes)\n'
 support+='\n'+'\n'.join(extract(xec,sig)for sig in('static void\nnewarray(','OP(newaz)','static uchar*\narrayspan(','OP(slicea)'))
 support+='\n'+'\n'.join(extract(xec,sig)for sig in('static uchar*\narrayaddress(','static void\narrayindex(','OP(indb)','OP(indw)','OP(movb)','OP(cvtbw)','OP(cvtwb)'))
 names=('cvtcw','cvtcf','cvtwc','cvtlc','cvtfc','cvtca','cvtac')
 support+='\n'+'\n'.join(extract(strings,'OP('+n+')')for n in names)+'\n'+extract(xec,'OP(cvtcl)')+'\n'+extract(xec,'OP(lena)')+'\n'
 handlers='\n'+'\n'.join('optab[I'+n.upper()+']=actual_'+n+';'for n in names+('cvtcl','lena','newaz','slicea','indb','indw','movb','cvtbw','cvtwb'))
 return source+'\n'+support,handlers
