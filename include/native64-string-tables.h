#ifndef NATIVE64_STRING_TABLES_H
#define NATIVE64_STRING_TABLES_H
#include "vmfetch.h"
#include "native64-local-bind.h"

/* Bounded valid UTF-8 only; no allocation or locale-sensitive comparison. */
static int
native64moduletext(const uchar *p,unsigned int n)
{
    unsigned int i=0,c,k,minimum,j;
    if(p==nil || p==H || n>4096)return 0;
    while(i<n){
        c=p[i++];if(c<128)continue;
        if(c>=0xc2 && c<=0xdf){c&=31;k=1;minimum=128;}
        else if(c>=0xe0 && c<=0xef){c&=15;k=2;minimum=2048;}
        else if(c>=0xf0 && c<=0xf4){c&=7;k=3;minimum=65536;}
        else return 0;
        if(k>n-i)return 0;
        for(j=0;j<k;j++){if((p[i]&0xc0)!=0x80)return 0;c=(c<<6)|(p[i++]&63);}
        if(c<minimum || c>0x10ffff || (c>=0xd800 && c<=0xdfff))return 0;
    }
    return 1;
}

static int
native64moduletextcmp(const uchar *a,unsigned int na,const uchar *b,unsigned int nb)
{
    int order=memcmp(a,b,na<nb?na:nb);
    return order?order:na<nb?-1:na>nb;
}

static int
native64modulestringscheck(const Native64ModuleSpec *s)
{
    unsigned int i,total=0,end=0;const Native64ModuleString *v;
    if(s->nstrings>65536 || (s->nstrings && (s->strings==nil || s->strings==H || s->string==nil || s->string==(String*(*)(char*,int))H)))return 0;
    for(i=0;i<s->nstrings;i++){
        v=s->strings+i;
        if(v->offset<end || v->offset%8 || v->offset>s->databytes || 8>s->databytes-v->offset ||
           !(s->datamap[v->offset/64]&(0x80>>((v->offset/8)%8))) || !native64moduletext(v->text,v->bytes) ||
           v->bytes>1024*1024-total)return 0;
        total+=v->bytes;end=v->offset+8;
    }
    return 1;
}

/* Mixed-width String table C bridge, selected only by the trusted constructor.
 * Search/validation cannot allocate or yield. Decode/PC publication occurs only
 * after all comparisons and the selected full Inst span have succeeded. */
static int
native64modulecasecstep(void)
{
    VmFetch fetched;Inst in;Module *m;Type *data,*frame;uintptr mp,source,base,program,pc;
    unsigned int count,bytes,header,lo,n,half,at,selected,i,expected,bit;String *value,*low,*high;int order;
    if(!vmfetch(&fetched) || fetched.op!=ICASEC)return 0;
    m=fetched.module;
    if(!native64model() || m->cstep!=native64typedlea8step || m->native64casec!=native64modulecasecstep ||
       m->compiled || m->ntype<=0 || m->mptype<0 || m->mptype>=m->ntype || m->type==nil || m->type==H)error(exModule);
    data=m->type[m->mptype];
    if(data==nil || data==H || data->ref<=0 || data->size<8 || data->size>1024*1024 || data->np<0 || data->np>(data->size+63)/64)error(exHeap);
    memmove(&in,fetched.pc,sizeof(in));
    if(UDST(in.add)!=AMP || (USRC(in.add)!=AFP && USRC(in.add)!=AMP) || (in.add&ARM)!=AXNON || in.reg ||
       in.d.ind<0 || in.d.ind%8 || (unsigned int)in.d.ind>(unsigned int)data->size-8 || in.s.ind<0 || in.s.ind%8)error(exBounds);
    mp=(uintptr)R.MP;
    if(!mp || R.MP==H || mp>(uintptr)-1-data->size)error(exBounds);
    if(USRC(in.add)==AMP){base=mp;frame=data;}
    else{
        base=(uintptr)R.FP;
        if(!base || R.FP==H || base%8 || base>(uintptr)-1-sizeof(Frame))error(exBounds);
        memmove(&frame,R.FP+O(Frame,t),sizeof(frame));
        if(frame==nil){
            if(base<OA(Stkext,reg.tos.fu))error(exBounds);
            memmove(&frame,(void*)(base-OA(Stkext,reg.tos.fu)+O(Stkext,reg.TR)),sizeof(frame));
        }
        if(!native64localtype(frame) || in.s.ind<64)error(exBounds);
    }
    bit=in.s.ind/8;
    if((unsigned int)in.s.ind>(unsigned int)frame->size || 8>(unsigned int)frame->size-in.s.ind ||
       bit/8>=(unsigned int)frame->np || !(frame->map[bit/8]&(0x80>>(bit%8))) || base>(uintptr)-1-frame->size)error(exBounds);
    source=base+in.s.ind;memmove(&value,(void*)source,8);
    base=mp+in.d.ind;memmove(&count,(void*)base,4);
    if(count>4096)error(exBounds);
    bytes=count?12+24*count:8;header=count?8:4;
    if(bytes>(unsigned int)data->size-in.d.ind)error(exBounds);
    for(i=0;i<bytes;i+=4){
        expected=count && i>=8 && i<8+24*count && (i-8)%24<16;
        bit=(in.d.ind+i)/8;
        if(!!(bit/8<(unsigned int)data->np && (data->map[bit/8]&(0x80>>(bit%8))))!=expected)error(exBounds);
    }
    memmove(&selected,(void*)(base+header+24*count),4);
    lo=0;n=count;
    while(n){
        half=n/2;at=8+24*(lo+half);memmove(&low,(void*)(base+at),8);order=stringcmp(value,low);
        if(order<0){n=half;continue;}
        if(order){
            memmove(&high,(void*)(base+at+8),8);
            if(high==H || stringcmp(value,high)>0){lo+=half+1;n-=half+1;continue;}
        }
        memmove(&selected,(void*)(base+at+16),4);break;
    }
    program=(uintptr)m->prog;
    if(m->nprog<=0 || selected>=(unsigned int)m->nprog || !program || m->prog==H ||
       selected>(uintptr)-1/sizeof(Inst) || program>(uintptr)-1-(uintptr)selected*sizeof(Inst)-sizeof(Inst))error(exBounds);
    pc=program+(uintptr)selected*sizeof(Inst);
    vmdecode(&fetched);R.PC=(Inst*)pc;return 1;
}
#endif
