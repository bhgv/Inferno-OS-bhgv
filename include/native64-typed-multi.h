#ifndef NATIVE64_TYPED_MULTI_H
#define NATIVE64_TYPED_MULTI_H
#include "native64-typed-code.h"
typedef struct Native64TypedCall Native64TypedCall;
struct Native64TypedCall {
    unsigned int first,end;
    Type *callee;
    Inst *target;
    unsigned int ordinal,contract,inherited;
    unsigned int external,modulemode,moduleoffset;
};

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
        c=calls+i;width=c->contract&15;
        if(c->first>=count || c->end>=count-1 || c->end<=c->first || (i && c->first<=calls[i-1].end) ||
           c->callee==nil || c->callee==H || (!c->external && (c->target==nil || c->target==H)) || c->ordinal>536870911 || c->inherited>1 ||
           c->external>1 || (c->external && (c->ordinal>32767 || (c->modulemode!=AFP && c->modulemode!=AMP) || c->moduleoffset%8 ||
           !native64typedcodeoperand(c->modulemode,c->moduleoffset,fpbytes,mpbytes) ||
           8>(c->modulemode==AFP?fpbytes:mpbytes)-c->moduleoffset)) ||
           c->callee->size<64 || c->callee->size>65536 || c->callee->size%8 ||
           (c->contract && ((c->contract&~31) || (width!=1 && width!=4 && width!=8) || ((c->contract&16) && width!=8))) ||
           (c->inherited && !c->contract) || (c->contract && c->end<c->first+2))return 0;
        s=steps+c->first;
        if(c->external){
            if(s->op!=IMFRAME || s->mode!=(SRC(c->modulemode)|AXIMM|DST(AFP)) || s->s!=c->moduleoffset || s->m!=c->ordinal || s->d!=40 || s->returned)return 0;
        }else if(s->op!=IFRAME || s->mode!=(SRC(AIMM)|DST(AFP)) || s->m || s->d!=40 || s->returned)return 0;
        stop=c->end-(c->contract!=0);
        for(j=c->first+1;j<stop;j++){
            s=steps+j;sm=USRC(s->mode);width=s->op==IMOVB?1:s->op==IMOVW?4:8;
            if((s->op!=IMOVB && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF && s->op!=IMOVP) ||
               s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || s->returned!=64 || sm==AXXX ||
               !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || s->d<64 || s->d>65535 ||
               (unsigned long)s->d>(unsigned int)c->callee->size || width>(unsigned int)c->callee->size-s->d || s->d%width)return 0;
        }
        if(c->contract){
            s=steps+c->end-1;width=c->contract&15;
            if(s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->m || s->d!=32)return 0;
            if(c->inherited){if(s->op!=IMOVL || s->s!=32 || s->returned!=64)return 0;}
            else if(s->op!=ILEA || s->returned!=(128|c->contract) || s->s<64 || (unsigned long)s->s>fpbytes || width>fpbytes-s->s || s->s%width)return 0;
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
                else{p->s.imm=s->s;p->d.i.f=40;p->d.i.s=s->d;if(c->contract && !c->inherited && j==c->end-1)p->reg=c->contract;}
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
