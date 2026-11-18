#ifndef NATIVE64_TYPED_MULTI_H
#define NATIVE64_TYPED_MULTI_H
#include "native64-typed-code.h"
#include "native64-stat-result.h"
typedef struct Native64TypedCall Native64TypedCall;
struct Native64TypedCall {
    unsigned int first,end;
    Type *callee;
    Inst *target;
    unsigned int ordinal,contract,inherited;
    unsigned int external,modulemode,moduleoffset;
};

/* Numeric Draw.Point/Rect copies into a prepared imported frame. */
static int
native64typedgeometryarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{
 unsigned int sm=USRC(s->mode),i,bit,sourcebytes=sm==AFP?fp:mp;
 if(s->op!=IMOVM || s->mode!=(SRC(sm)|AXIMM|DST(AFP|AIND)) || (sm!=AFP && sm!=AMP) || s->returned!=64 ||
    (s->m!=8 && s->m!=16) || s->s<0 || s->s%4 || (sm==AFP && s->s<64) || (unsigned long)s->s>sourcebytes ||
    (unsigned long)s->m>sourcebytes-s->s || s->d<64 || s->d%4 || s->d>65535 || (unsigned long)s->d>bytes || (unsigned long)s->m>bytes-s->d ||
    map==nil || map==H || np!=(bytes+63)/64)return 0;
 for(i=0;i<(unsigned long)s->m;i+=4){bit=(s->d+i)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
 return 1;
}

static int
native64typedfieldarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{
 unsigned int sm=USRC(s->mode),bit;
 if(s->op!=IMOVP || s->returned!=Native64TypedFieldArg || s->mode!=(SRC(sm)|DST(AFP|AIND)) ||
    (sm!=(AFP|AIND) && sm!=(AMP|AIND)) || !native64typedfieldoffset(s->m,1) || !native64typedrootoperand(sm&~AIND,s->s,fp,mp) ||
    s->d<64 || s->d%8 || s->d>65535 || (unsigned long)s->d>bytes || 8>bytes-s->d || map==nil || map==H)return 0;
 bit=s->d/8;return bit/8<np && (map[bit/8]&(128>>(bit%8)));
}

enum { Native64TypedListPrep=112, Native64TypedArgClear=126 };
static int
native64typedlistprep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 Native64TypedStep copy=*s;Inst probe;
 if(s->returned!=Native64TypedListPrep || (s->op!=IMOVP && s->op!=ICONSP) || UDST(s->mode)!=AFP ||
    (s->op==IMOVP && USRC(s->mode)!=AMP) || s->m || (s->mode&ARM)!=AXNON)return 0;
 copy.returned=0;return native64typedcode(&probe,1,&copy,1,fp,mp) &&
  native64typedrootoperand(USRC(s->mode),s->s,fp,mp) && native64typedrootoperand(AFP,s->d,fp,mp);
}

/* One computed String argument in an already prepared imported frame. */
static int
native64typedconcatarg(const Native64TypedStep *s,unsigned int fpbytes,unsigned int mpbytes,unsigned int bytes,const uchar *map,unsigned int np)
{
    unsigned int mm=s->mode&ARM,bit;
    if(s->op!=IADDC || s->mode>255 || s->returned!=64 || UDST(s->mode)!=(AFP|AIND) ||
       (mm!=AXINF && mm!=AXINM) || s->m<0 || s->m>65535 ||
       !native64typedrootoperand(USRC(s->mode),s->s,fpbytes,mpbytes) ||
       !native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes) ||
       s->d<64 || s->d%8 || (unsigned long)s->d>bytes || 8>bytes-s->d || map==nil)return 0;
    bit=s->d/8;return bit/8<np && (map[bit/8]&(0x80>>(bit%8)));
}

static int
native64typedargprep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 if(s->returned)return 0;
 if(s->op==ISLICEA)return native64typedbytestep(s,fp,mp);
 return s->op==IMOVP && s->mode==(SRC(USRC(s->mode))|DST(AFP)) && !s->m &&
  native64typedrootoperand(USRC(s->mode),s->s,fp,mp) && native64typedrootoperand(AFP,s->d,fp,mp);
}

static int
native64typedcomputedarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{
 unsigned int bit,width=s->op==ILENA?4:8,root=s->op==ICVTAC;
 if((s->op!=ILENA && s->op!=ICVTAC) || s->mode!=(SRC(USRC(s->mode))|DST(AFP|AIND)) || s->m || s->returned!=64 ||
    !native64typedrootoperand(USRC(s->mode),s->s,fp,mp) || s->d<64 || s->d%width || (unsigned long)s->d>bytes || width>bytes-s->d || map==nil || map==H)return 0;
 bit=s->d/8;return bit/8<np && !!(map[bit/8]&(0x80>>(bit%8)))==root;
}

static int
native64typedblocked(unsigned int pc,const Native64TypedCall *calls,unsigned int n)
{
    unsigned int lo=0,hi=n,mid;
    while(lo<hi){mid=lo+(hi-lo)/2;
        if(pc<=calls[mid].first)hi=mid;
        else if(pc>calls[mid].end)lo=mid+1;
        else return 1;
    }
    return 0;
}

/* A complete immutable checked-IR function and its sorted disjoint call sites.
 * Exact semantic fields and Type/target identities remain trusted compiler/
 * builder premises. All structural checks precede output; no allocation. */
static int
native64typedmulticode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int fpbytes,unsigned int mpbytes,const Native64TypedCall *calls,unsigned int n)
{
    const Native64TypedCall *c;const Native64TypedStep *s;Native64TypedStep checkstep;
    Inst probe,*p;unsigned int i,j,k=0,stop,sm,width;
    if(!n)return native64typedcode(out,capacity,steps,count,fpbytes,mpbytes);
    if(!native64model() || out==nil || steps==nil || calls==nil || !count || count>65536 || capacity<count || n>count/2 ||
       fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8)return 0;
    for(i=0;i<n;i++){
        c=calls+i;width=c->contract==88?88:c->contract==48?16:c->contract&15;
        if(c->first>=count || c->end>=count-1 || c->end<=c->first || (i && c->first<=calls[i-1].end) ||
           c->callee==nil || c->callee==H || (!c->external && (c->target==nil || c->target==H)) || c->ordinal>536870911 || c->inherited>1 ||
           c->external>1 || (c->external && (c->ordinal>32767 || (c->modulemode!=AFP && c->modulemode!=AMP) || c->moduleoffset%8 ||
           !native64typedcodeoperand(c->modulemode,c->moduleoffset,fpbytes,mpbytes) ||
           8>(c->modulemode==AFP?fpbytes:mpbytes)-c->moduleoffset)) ||
           c->callee->size<64 || c->callee->size>65536 || c->callee->size%8 ||
           (c->contract==48 && c->external) || (c->contract==88 && (!c->external || c->inherited || c->callee->size!=72 || c->callee->np!=2 || c->callee->map[0] || c->callee->map[1]!=0x80)) || (c->contract && c->contract!=48 && c->contract!=88 && ((c->contract&~31) || (width!=1 && width!=4 && width!=8) || ((c->contract&16) && width!=8))) ||
           (c->inherited && !c->contract) || (c->contract && c->end<c->first+2))return 0;
        s=steps+c->first;
        if(c->external){
            if(s->op!=IMFRAME || s->mode!=(SRC(c->modulemode)|AXIMM|DST(AFP)) || s->s!=c->moduleoffset || s->m!=c->ordinal || s->d!=40 || s->returned)return 0;
        }else if(s->op!=IFRAME || s->mode!=(SRC(AIMM)|DST(AFP)) || s->m || s->d!=40 || s->returned)return 0;
        stop=c->end-(c->contract!=0);
        for(j=c->first+1;j<stop;j++){
            s=steps+j;sm=USRC(s->mode);width=s->op==IMOVB?1:s->op==IMOVW?4:8;
            if(!s->returned){if(c->external || !native64typedargprep(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==Native64TypedFieldArg){
                if(c->callee->np<0 || !native64typedfieldarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;continue;
            }
            if(s->returned==Native64TypedArgClear){
                checkstep=*s;checkstep.returned=Native64TypedListPrep;
                if(s->op!=IMOVP || !native64typedlistprep(&checkstep,fpbytes,mpbytes))return 0;continue;
            }
            if(s->returned==Native64TypedListPrep){
                if(!c->external || !native64typedlistprep(s,fpbytes,mpbytes))return 0;continue;
            }
            if(s->op==IMOVM){
                if(!c->external || c->callee->np<0 || !native64typedgeometryarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if(s->op==ILENA || s->op==ICVTAC){
                if((s->op==ILENA)!=c->external || c->callee->np<0 || !native64typedcomputedarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if(s->op==IADDC){
                if((!c->external && (!c->inherited || c->target!=out)) || c->callee->np<0 || !native64typedconcatarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if((s->op!=IMOVB && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF && s->op!=IMOVP) ||
               s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || s->returned!=64 || sm==AXXX ||
               !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || s->d<64 || s->d>65535 ||
               (unsigned long)s->d>(unsigned int)c->callee->size || width>(unsigned int)c->callee->size-s->d || s->d%width)return 0;
        }
        if(c->contract){
            s=steps+c->end-1;width=c->contract==88?88:c->contract==48?16:c->contract&15;
            if(s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->m || s->d!=32)return 0;
            if(c->inherited){if(s->op!=IMOVL || s->s!=32 || s->returned!=64)return 0;}
            else if(s->op!=ILEA || s->returned!=(128|c->contract) || s->s<64 || (unsigned long)s->s>fpbytes || width>fpbytes-s->s || s->s%((c->contract==48 || c->contract==88)?8:width))return 0;
        }
        s=steps+c->end;
        if(c->external){
            if(s->op!=IMCALL || s->mode!=(SRC(AFP)|AXIMM|DST(c->modulemode)) || s->m!=c->ordinal || s->s!=40 || s->d!=c->moduleoffset || s->returned!=(c->inherited?32:0))return 0;
        }else if(s->op!=ICALL || s->mode!=(SRC(AFP)|DST(AIMM)) || s->m || s->s!=40 || s->returned!=(c->inherited?32:0))return 0;
    }
    for(i=0;i<count;){
        if(k<n && i==calls[k].first){i=calls[k++].end+1;continue;}
        checkstep=steps[i];
        if(native64typedbranch(checkstep.op)){
            if(checkstep.d<0 || (unsigned long)checkstep.d>=count || native64typedblocked(checkstep.d,calls,n))return 0;
            checkstep.d=0;
        }
        if(!native64typedcode(&probe,1,&checkstep,1,fpbytes,mpbytes))return 0;
        i++;
    }
    k=0;
    for(i=0;i<count;){
        if(k<n && i==calls[k].first){
            c=calls+k++;
            for(j=i;j<=c->end;j++){
                s=steps+j;p=out+j;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;
                if(j==i){p->s.imm=c->external?c->moduleoffset:c->ordinal;p->reg=c->external?c->ordinal:0;p->d.ind=40;}
                else if(j==c->end){p->s.ind=40;if(c->external){p->reg=c->ordinal;p->d.ind=c->moduleoffset;}else p->d.ins=c->target;}
                else if(s->returned==Native64TypedFieldArg){p->s.i.f=s->s;p->s.i.s=s->m;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==Native64TypedArgClear){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==Native64TypedListPrep){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(!s->returned){native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else{p->s.imm=s->s;p->d.i.f=40;p->d.i.s=s->d;if(s->op==IADDC || s->op==IMOVM)p->reg=s->m;if(c->contract && !c->inherited && j==c->end-1)p->reg=c->contract;}
            }
            i=c->end+1;continue;
        }
        s=steps+i;checkstep=*s;if(native64typedbranch(s->op))checkstep.d=0;
        native64typedcode(out+i,1,&checkstep,1,fpbytes,mpbytes);
        if(native64typedbranch(s->op))out[i].d.ins=out+s->d;
        i++;
    }
    return 1;
}
#endif
