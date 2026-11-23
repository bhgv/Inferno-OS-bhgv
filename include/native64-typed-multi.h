#ifndef NATIVE64_TYPED_MULTI_H
#define NATIVE64_TYPED_MULTI_H
#include "native64-typed-code.h"
#include "native64-stat-result.h"
#include "native64-wm-tuple.h"
#include "native64-geometry-result.h"
typedef struct Native64TypedCall Native64TypedCall;
struct Native64TypedCall {
    unsigned int first,end;
    Type *callee;
    Inst *target;
    unsigned int ordinal,contract,inherited;
    unsigned int external,modulemode,moduleoffset,resultcontract,callerresultcontract; /* Separate result proof: numeric12/mixed16, roots48/72, geometry96/112. */
};

/* Exact numeric Image/Window field displacements; Word leaves stay 32-bit. */
static unsigned int
native64imagegeometryoffset(unsigned int tag)
{
 static const unsigned int offsets[]={0,16,0,8,16,24};return tag>=160 && tag<=165?offsets[tag-160]:tag==217 || tag==218?8:tag==219?16:tag==227?0:tag==230?4:~0U;
}

/* Numeric Draw.Point/Rect copies into a prepared imported frame. */
static int
native64typedgeometryarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{
 unsigned int sm=USRC(s->mode),i,bit,sourcebytes=sm==AFP?fp:mp,width=s->returned==84?s->m&255:s->m;
 if(s->op==IHEADM){if(s->returned!=64||s->m!=16||s->mode!=(SRC(sm)|AXIMM|DST(AFP|AIND))||!native64typedrootoperand(sm,s->s,fp,mp)||s->d<64||s->d%4||s->d>65535||(unsigned long)s->d>bytes||16>bytes-s->d||map==nil||map==H||np!=(bytes+63)/64)return 0;}
 else if((s->returned==84||native64imagegeometryoffset(s->returned)!=~0U) && (sm==(AFP|AIND) || sm==(AMP|AIND))){
  if(s->op!=IMOVM || s->mode!=(SRC(sm)|AXIMM|DST(AFP|AIND)) || (s->returned==84?!native64windowgeometryfield((unsigned long)s->m>>8,width):s->m!=(s->returned<162 || s->returned==217 || s->returned==227?16:8)) || !native64typedrootoperand(sm&~AIND,s->s,fp,mp) || s->d<64 || s->d%4 || s->d>65535 || (unsigned long)s->d>bytes || width>bytes-s->d || map==nil || map==H || np!=(bytes+63)/64)return 0;
 }else if(s->op!=IMOVM || s->mode!=(SRC(sm)|AXIMM|DST(AFP|AIND)) || (sm!=AFP && sm!=AMP) || s->returned!=64 ||
    (s->m!=8 && s->m!=12 && s->m!=16) || s->s<0 || s->s%4 || (sm==AFP && s->s<64) || (unsigned long)s->s>sourcebytes ||
    (unsigned long)s->m>sourcebytes-s->s || s->d<64 || s->d%4 || s->d>65535 || (unsigned long)s->d>bytes || width>bytes-s->d ||
    map==nil || map==H || np!=(bytes+63)/64)return 0;
 for(i=0;i<width;i+=4){bit=(s->d+i)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
 return 1;
}

static int
native64typedfieldarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{
 unsigned int sm=USRC(s->mode),bit;
 if(s->op!=IMOVP || (s->returned!=Native64TypedFieldArg && s->returned!=233) || s->mode!=(SRC(sm)|DST(AFP|AIND)) ||
    (sm!=(AFP|AIND) && sm!=(AMP|AIND)) || (s->returned==233?s->m!=40:!native64typedfieldoffset(s->m,1)) || !native64typedrootoperand(sm&~AIND,s->s,fp,mp) ||
    s->d<64 || s->d%8 || s->d>65535 || (unsigned long)s->d>bytes || 8>bytes-s->d || map==nil || map==H)return 0;
 bit=s->d/8;return bit/8<np && (map[bit/8]&(128>>(bit%8)));
}

enum { Native64TypedListPrep=112, Native64TypedArgClear=126 };
static int
native64typedlistprep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 Native64TypedStep copy=*s;Inst probe;
 if(s->returned!=Native64TypedListPrep || (s->op!=IMOVP && s->op!=ICONSP && s->op!=ICONSW) || UDST(s->mode)!=AFP ||
    (s->op==IMOVP && USRC(s->mode)!=AMP) || s->m || (s->mode&ARM)!=AXNON)return 0;
 copy.returned=0;return native64typedcode(&probe,1,&copy,1,fp,mp) &&
  (s->op==ICONSW?native64typedwordoperand(USRC(s->mode),s->s,fp,mp):native64typedrootoperand(USRC(s->mode),s->s,fp,mp)) && native64typedrootoperand(AFP,s->d,fp,mp);
}

static int
native64typedlistprefix(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 Native64TypedStep copy=*s;Inst probe;copy.returned=0;
 return s->returned==187 && (s->op==IMOVP || s->op==ICONSP) && UDST(s->mode)==AFP && (s->mode&ARM)==AXNON && !s->m &&
  native64typedcode(&probe,1,&copy,1,fp,mp) && native64typedrootoperand(USRC(s->mode),s->s,fp,mp) && native64typedrootoperand(AFP,s->d,fp,mp);
}

/* 180..182 are private preparation roles; 176 is reserved for 128|48 pair binding. */
static int
native64typedstringprep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 Native64TypedStep copy=*s;Inst probe;
 if(s->returned==63)return s->op==ICVTWC&&native64typedwindowcodec(s,fp,mp);
 if(s->returned==44){copy.returned=0;return s->op==IHEADP&&s->mode==(SRC(USRC(s->mode))|DST(AFP))&&!s->m&&native64typedrootoperand(USRC(s->mode),s->s,fp,mp)&&native64typedrootoperand(AFP,s->d,fp,mp)&&native64typedcode(&probe,1,&copy,1,fp,mp);}
 if(s->returned==181){copy.returned=Native64TypedListPrep;return s->op==IMOVP && native64typedlistprep(&copy,fp,mp);}
 if((s->returned!=180&&s->returned!=87) || (s->returned==87?(s->op!=ILENC&&s->op!=IMOVP&&s->op!=ISLICEC):(s->op!=ICVTWC && s->op!=IADDC)) || UDST(s->mode)!=AFP)return 0;
 copy.returned=0;return native64typedcode(&probe,1,&copy,1,fp,mp);
}

/* One computed String argument in an already prepared imported frame. */
static int
native64typedbytestringarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{unsigned int bit;if(s->returned!=70||s->op!=ICVTAC||s->mode!=(SRC(AFP)|DST(AFP|AIND))||s->m||!native64typedrootoperand(AFP,s->s,fp,mp)||!native64typedrootoperand(AFP,s->d,bytes,0)||s->d>65535||map==nil||map==H||np!=(bytes+63)/64)return 0;bit=s->d/8;return !!(map[bit/8]&(128>>(bit%8)));}
static int
native64typedimagewordarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{Native64TypedStep c=*s;unsigned int bit;if(s->returned!=67||s->op!=IMOVW||(unsigned long)s->s>>16>40||UDST(s->mode)!=(AFP|AIND)||!native64typedwordoperand(AFP,s->d,bytes,0)||s->d>65535||map==nil||map==H||np!=(bytes+63)/64)return 0;c.returned=133;c.mode=(c.mode&~7)|AFP;c.d=64;if(!native64typedimageword(&c,fp,mp))return 0;bit=s->d/8;return !(map[bit/8]&(128>>(bit%8)));}
/* Immediate array element reads; no borrowed address survives the next step. */
static int
native64typedstringarrayarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{unsigned int bit,mm=s->mode&ARM;Native64TypedStep copy=*s;
 if(s->returned!=74&&s->returned!=76)return 0;
 if(s->op==IINDL){copy.returned=255;return native64typedrefarray(&copy,fp,mp);}
 if(UDST(s->mode)!=(AFP|AIND)||s->s!=48||!native64typedrootoperand(AFP,s->d,bytes,0)||s->d>65535||map==nil||map==H||np!=(bytes+63)/64)return 0;bit=s->d/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
 if(s->returned==74)return s->op==IMOVP&&s->mode==(SRC(AFP|AIND)|DST(AFP|AIND))&&!s->m;
 return s->op==IADDC&&s->mode==(SRC(AFP|AIND)|mm|DST(AFP|AIND))&&(mm==AXINF||mm==AXINM)&&native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fp,mp);}
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
native64typedarrayscreen(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{if(s->returned!=143)return 0;if(s->op==IINDL){Native64TypedStep copy=*s;copy.returned=127;return native64typedarraycall(&copy,fp,mp);}return s->op==IMOVP&&s->mode==(SRC(AFP|AIND)|DST(AFP))&&s->s==56&&!s->m&&native64typedrootoperand(AFP,s->d,fp,mp);}

static int
native64typedcomputedarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{
 unsigned int bit,width=s->op==ILENA?4:8,root=s->op!=ILENA;
 if((s->op!=ILENA && s->op!=ICVTAC && s->op!=ICVTCA) || s->mode!=(SRC(USRC(s->mode))|DST(AFP|AIND)) || s->m || s->returned!=64 ||
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
/* A branch is checked against its real target in the complete body first.
 * A two-instruction probe avoids inventing a self-entry into a protected
 * registry suffix while validating/materializing that branch in isolation. */
static int
native64typedbranchprobe(Inst*out,const Native64TypedStep*step,unsigned int fp,unsigned int mp)
{Native64TypedStep pair[2];Inst code[2];pair[0]=*step;pair[0].d=1;pair[1]=(Native64TypedStep){IRET,SRC(AXXX)|DST(AXXX),0,0,0,0};if(!native64typedcode(code,2,pair,2,fp,mp))return 0;if(out){*out=code[0];out->d.ins=nil;}return 1;}

static int
native64typedmulticode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int fpbytes,unsigned int mpbytes,const Native64TypedCall *calls,unsigned int n)
{
    const Native64TypedCall *c;const Native64TypedStep *s;Native64TypedStep checkstep;
    Inst probe,*p;unsigned int i,j,k=0,stop,sm,width;
    if(!n)return native64typedcode(out,capacity,steps,count,fpbytes,mpbytes);
    if(!native64model() || out==nil || steps==nil || calls==nil || !count || count>65536 || capacity<count || n>count/2 ||
       fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8)return 0;
    for(i=0;i<n;i++){
        c=calls+i;width=native64wmtuplewidth(c->contract)?native64wmtuplewidth(c->contract):native64geometryresultwidth(c->contract)?native64geometryresultwidth(c->contract):c->contract==16?16:c->contract==72?24:c->contract==88?88:c->contract==48?16:c->contract&15;
        if(c->first>=count || c->end>=count-1 || c->end<=c->first || (i && c->first<=calls[i-1].end) ||
           c->callee==nil || c->callee==H || (!c->external && (c->target==nil || c->target==H)) || c->ordinal>536870911 || c->inherited>1 ||
           c->external>1 || (c->external && (c->ordinal>32767 || !native64librarymoduleoperand(c->modulemode,c->moduleoffset,fpbytes,mpbytes))) ||
           c->callee->size<64 || c->callee->size>65536 || c->callee->size%8 ||
           ((c->contract==16)!=(c->resultcontract==16)) || (c->contract==16 && c->inherited && c->callerresultcontract!=16) || ((c->contract==72)!=(c->resultcontract==72)) || (c->resultcontract && c->resultcontract!=12 && c->resultcontract!=16 && c->resultcontract!=48 && c->resultcontract!=72 && !native64wmtuplewidth(c->resultcontract) && !native64geometryresultwidth(c->resultcontract)) ||
           ((native64geometryresultwidth(c->contract) || native64geometryresultwidth(c->resultcontract)) && (c->contract!=c->resultcontract || (c->inherited && c->callerresultcontract!=c->contract && !(c->external&&c->contract==96&&c->callerresultcontract==112)))) ||
           ((native64wmtuplewidth(c->contract)||native64wmtuplewidth(c->resultcontract))&&(c->contract!=c->resultcontract||(c->inherited&&c->callerresultcontract!=c->contract))) || ((c->contract==48)!=(c->resultcontract==48)) || (c->contract==48 && c->inherited && c->callerresultcontract!=48) || (c->contract==88 && (!c->external || c->inherited || c->callee->size!=72 || c->callee->np!=2 || c->callee->map[0] || c->callee->map[1]!=0x80)) || (c->contract && !native64wmtuplewidth(c->contract) && !native64geometryresultwidth(c->contract) && c->contract!=12 && c->contract!=16 && c->contract!=48 && c->contract!=72 && c->contract!=88 && ((c->contract&~31) || (width!=1 && width!=4 && width!=8) || ((c->contract&16) && width!=8))) ||
           (c->inherited && !c->contract) || (c->contract && c->end<c->first+2))return 0;
        s=steps+c->first;
        if(c->external){
            if(s->op!=IMFRAME || s->mode!=(SRC(c->modulemode)|AXIMM|DST(AFP)) || s->s!=c->moduleoffset || s->m!=c->ordinal || s->d!=40 || s->returned)return 0;
        }else if(s->op!=IFRAME || s->mode!=(SRC(AIMM)|DST(AFP)) || s->m || s->d!=40 || s->returned)return 0;
        stop=c->end-(c->contract!=0);
        for(j=c->first+1;j<stop;j++){
            s=steps+j;sm=USRC(s->mode);width=s->op==IMOVB?1:s->op==IMOVW?4:8;
            if(!s->returned){if(c->external || !native64typedargprep(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==Native64TypedRecordArg){
                if(!native64recordarglayout(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if(s->returned==Native64TypedArrayArgIndex){
                if(c->external||j+2>=stop||!native64rawargindexchain(s,fpbytes,mpbytes))return 0;
                continue;
            }
            if(s->returned==80||s->returned==Native64TypedBorrowIndex){if(!native64typedrawindex(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==65){
                unsigned int n=native64rawusewidth(s),bit,at;
                int root=s->op==IMOVP;
                if(!native64typedrawuse(s,fpbytes,mpbytes)||USRC(s->mode)!=(AFP|AIND)||
                   !native64rawslot(s->s)||UDST(s->mode)!=(AFP|AIND)||s->d<64||
                   (root&&s->d%8)||(unsigned long)s->d>(unsigned int)c->callee->size||
                   n>(unsigned int)c->callee->size-s->d)return 0;
                for(at=0;at<n;at++){
                    bit=(s->d+at)/8;
                    if(!!(bit/8<(unsigned int)c->callee->np&&(c->callee->map[bit/8]&(128>>(bit%8))))!=root)return 0;
                }
                continue;
            }
            if(s->returned==86){if(!native64typedwindowwordarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;continue;}
            if(s->returned==153){if(native64typedbranch(s->op) || !native64typedlibrary(s,fpbytes,mpbytes) || ((UDST(s->mode)&AIND) && !(s->m&0xff0000)))return 0;continue;}
            if(s->returned==154){
                if(c->external||steps[c->end].op!=ICALL||
                   !native64libraryprecall(steps,c->first,j,stop,fpbytes,mpbytes,
                                          c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if(s->returned==156){if(!native64libraryslicearg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np) || j==c->first+1 || steps[j-1].returned!=155 || steps[j-1].op!=IMOVP || steps[j-1].d!=s->d)return 0;continue;}
            if(s->returned==155){if(!native64typedlibraryarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;continue;}
            if(s->returned==46){Inst probe;if(j+1>=stop||steps[j+1].returned!=137||steps[j+1].op!=IINDC||steps[j+1].m!=s->d||!native64typedcode(&probe,1,s,1,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==152){unsigned int k,bit;
                if(c->external || s->op!=ICVTWF || s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || !native64typedwordoperand(sm,s->s,fpbytes,mpbytes) || !native64typedrootoperand(AFP,s->d,c->callee->size,0) || s->d>65535)return 0;
                for(k=0;k<8;k++){bit=(s->d+k)/8;if(bit/8<(unsigned int)c->callee->np && (c->callee->map[bit/8]&(128>>(bit%8))))return 0;}continue;
            }
            if(s->returned==143){if(!c->external||!native64typedarrayscreen(s,fpbytes,mpbytes))return 0;
                if(s->op==IINDL){if(j+1>=stop||steps[j+1].returned!=143||steps[j+1].op!=IMOVP)return 0;}
                else{if(j==c->first+1||steps[j-1].returned!=143||steps[j-1].op!=IINDL||j+2>=stop||steps[j+1].returned!=125||steps[j+1].op!=IMOVP||steps[j+1].s!=s->d||steps[j+1].m!=56||steps[j+2].returned!=126||steps[j+2].d!=s->d)return 0;}continue;
            }
            if(s->returned==127){
                if(!c->external && steps[c->end].op!=ICALL)return 0;
                if(s->op==IINDL){if(!native64typedarraycall(s,fpbytes,mpbytes) || j+1>=stop || steps[j+1].returned!=127 || steps[j+1].op!=IMOVP)return 0;}
                else{unsigned int bit;if(s->op!=IMOVP || s->mode!=(SRC(AFP|AIND)|DST(AFP|AIND)) || s->s!=56 || s->m || !native64typedrootoperand(AFP,s->d,c->callee->size,0) || s->d>65535 || j==c->first+1 || steps[j-1].returned!=127 || steps[j-1].op!=IINDL || c->callee->np<0)return 0;bit=s->d/8;if(bit/8>=(unsigned int)c->callee->np || !(c->callee->map[bit/8]&(128>>(bit%8))))return 0;}
                continue;
            }
            if(s->returned==130 || s->returned==137){
                unsigned int mode=(s->mode&ARM)==AXIMM?AIMM:(s->mode&ARM)==AXINF?AFP:AMP,k,bit;
                if(!(s->returned==137?s->op==IINDC:native64typedwordargop(s->op)) || s->mode!=(SRC(sm)|((s->mode&ARM))|DST(AFP|AIND)) || (s->mode&ARM)==AXNON || !(s->returned==137?native64typedrootoperand(sm,s->s,fpbytes,mpbytes):native64typedwordoperand(sm,s->s,fpbytes,mpbytes)) ||
                   !native64typedwordoperand(mode,s->m,fpbytes,mpbytes) || (mode==AIMM?(s->m< -32768 || s->m>32767):s->m>65535) || !native64typedwordoperand(AFP,s->d,c->callee->size,0) || s->d>65535 || c->callee->np<0)return 0;
                for(k=0;k<4;k++){bit=(s->d+k)/8;if(bit/8<(unsigned int)c->callee->np && (c->callee->map[bit/8]&(128>>(bit%8))))return 0;}continue;
            }
            if(s->returned==253){
                if(!c->external && steps[c->end].op!=ICALL)return 0;
                if(s->op==ICVTCW){
                    unsigned int at,bit;if(c->callee->np<0 || (unsigned int)c->callee->np>((unsigned int)c->callee->size+63)/64)return 0;if(s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->m || !native64typedrootoperand(AFP,s->s,fpbytes,mpbytes) || !native64typedwordoperand(AFP,s->d,c->callee->size,0) || s->d>65535 || j==c->first+1 || j+1>=stop || steps[j-1].returned!=253 || steps[j-1].op!=IHEADP || steps[j-1].d!=s->s || steps[j+1].returned!=253 || steps[j+1].op!=IMOVP || steps[j+1].d!=s->s)return 0;
                    for(at=0;at<4;at++){bit=(s->d+at)/8;if(bit/8<(unsigned int)c->callee->np && (c->callee->map[bit/8]&(128>>(bit%8))))return 0;}
                }else{
                    if(!native64typedinthead(s,fpbytes,mpbytes))return 0;
                    if(s->op==ITAIL){if(j>c->first+1 && steps[j-1].returned==253 && steps[j-1].op==ITAIL && (USRC(s->mode)!=AFP || s->s!=steps[j-1].d || s->d!=steps[j-1].d))return 0;if(j+1>=stop || steps[j+1].returned!=253 || (steps[j+1].op!=ITAIL && steps[j+1].op!=IHEADP) || steps[j+1].s!=s->d || steps[j+1].d!=s->d)return 0;}
                    else if(s->op==IHEADP){if(j>c->first+1 && steps[j-1].returned==253 && steps[j-1].op==ITAIL && (USRC(s->mode)!=AFP || s->s!=steps[j-1].d || s->d!=steps[j-1].d))return 0;if(j+1>=stop || steps[j+1].returned!=253 || steps[j+1].op!=ICVTCW || steps[j+1].s!=s->d)return 0;}
                    else if(j==c->first+1 || steps[j-1].returned!=253 || steps[j-1].op!=ICVTCW || steps[j-1].s!=s->d)return 0;
                }
                continue;
            }
            if(s->returned==252){
                if(!c->external && steps[c->end].op!=ICALL)return 0;
                if(s->op==IHEADP || s->op==ITAIL && UDST(s->mode)==(AFP|AIND)){
                    if(s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->m || !native64typedrootoperand(AFP,s->s,fpbytes,mpbytes) || s->d<64 || s->d%8 || (unsigned long)s->d>(unsigned int)c->callee->size || 8>(unsigned int)c->callee->size-s->d || c->callee->np<0 || (unsigned long)(s->d/64)>=(unsigned int)c->callee->np || !(c->callee->map[s->d/64]&(128>>((s->d/8)%8))) || j==c->first+1 || j+1>=stop || steps[j-1].returned!=252 || steps[j-1].op!=ITAIL || steps[j-1].d!=s->s || steps[j+1].returned!=252 || steps[j+1].op!=IMOVP || steps[j+1].d!=s->s)return 0;
                }else{
                    if(!native64typedheadprep(s,fpbytes,mpbytes))return 0;
                    if(s->op==ITAIL){if(j>c->first+1 && steps[j-1].returned==252 && steps[j-1].op==ITAIL && (USRC(s->mode)!=AFP || s->s!=steps[j-1].d || s->d!=steps[j-1].d))return 0;if(j+1>=stop || steps[j+1].returned!=252 || (steps[j+1].op!=ITAIL && steps[j+1].op!=IHEADP) || steps[j+1].s!=s->d || (steps[j+1].op==ITAIL && UDST(steps[j+1].mode)==AFP && steps[j+1].d!=s->d))return 0;}
                    else if(j==c->first+1 || steps[j-1].returned!=252 || (steps[j-1].op!=IHEADP && !(steps[j-1].op==ITAIL && UDST(steps[j-1].mode)==(AFP|AIND))) || steps[j-1].s!=s->d)return 0;
                }
                continue;
            }
            if(s->returned==237){if(c->external || c->contract || c->inherited || !native64typedidstring(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==238){checkstep=*s;checkstep.returned=Native64TypedListPrep;if(c->external || s->op!=IMOVP || !native64typedlistprep(&checkstep,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==187){if(!c->external || !native64typedlistprefix(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==182){checkstep=*s;checkstep.returned=Native64TypedListPrep;if(c->external || s->op!=IMOVP || !native64typedlistprep(&checkstep,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==74||s->returned==76){if((s->returned==74?c->external:!c->external)||!native64typedstringarrayarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;if(s->op==IINDL){if(j+1>=stop||steps[j+1].returned!=s->returned||steps[j+1].s!=48)return 0;}else if(j==c->first+1||steps[j-1].returned!=s->returned||steps[j-1].op!=IINDL)return 0;continue;}
            if(s->returned==75){checkstep=*s;checkstep.returned=64;if(c->external||j+1!=c->end-(c->contract?1:0)||!native64typedconcatarg(&checkstep,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;continue;}
            if(s->returned==69||s->returned==71){if(!c->external||!native64typedbytestring(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==70){if(!c->external||!native64typedbytestringarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;continue;}
            if(s->returned==67){if(!c->external||!native64typedimagewordarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;continue;}
            if(s->returned==61||s->returned==66){if(!native64typedrootpath(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==44 || s->returned==63 || s->returned==87 || s->returned==180 || s->returned==181){if((!c->external&&s->returned!=181&&s->returned!=44) || !native64typedstringprep(s,fpbytes,mpbytes))return 0;continue;}
            if(s->returned==141){checkstep=*s;checkstep.returned=213;if(!c->external||s->op!=IHEADP||UDST(s->mode)!=AFP||!native64typedwindowlist(&checkstep,fpbytes,mpbytes)||j+2>=stop||steps[j+1].returned!=217||steps[j+1].s!=s->d||steps[j+2].returned!=126||steps[j+2].d!=s->d)return 0;continue;}
            if(s->returned==166 || s->returned==235){
                checkstep=*s;checkstep.returned=s->m==24?214:Native64TypedDraw;
                if(c->external!=(s->returned==166) || (s->returned==235?s->m!=8:s->m!=16 && s->m!=24) || USRC(s->mode)!=(AFP|AIND) && USRC(s->mode)!=(AMP|AIND) || UDST(s->mode)!=AFP || !(s->m==24?native64typedwmsrvwindow(&checkstep,fpbytes,mpbytes):native64typeddrawstep(&checkstep,fpbytes,mpbytes)))return 0;continue;
            }
            if(s->returned==Native64TypedFieldArg || s->returned==233){
                if((s->returned==233 && !c->external) || c->callee->np<0 || !native64typedfieldarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;continue;
            }
            if(s->returned==Native64TypedArgClear){
                checkstep=*s;checkstep.returned=Native64TypedListPrep;
                if(s->op!=IMOVP || !native64typedlistprep(&checkstep,fpbytes,mpbytes))return 0;continue;
            }
            if(s->returned==Native64TypedListPrep){
                if(!c->external || !native64typedlistprep(s,fpbytes,mpbytes))return 0;continue;
            }
            if(s->op==IMOVM||s->op==IHEADM){
                if(((s->returned==227 || s->returned==230) && !c->external) || (!c->external && steps[c->end].op!=ICALL) || c->callee->np<0 || !native64typedgeometryarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if(s->op==ILENA || s->op==ICVTAC || s->op==ICVTCA){
                if((s->op==ILENA)!=c->external || c->callee->np<0 || !native64typedcomputedarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if(s->op==IADDC){
                if((!c->external && !(j>c->first+1&&((steps[j-1].returned==153&&steps[j-1].op==IMOVP&&steps[j-1].m==(12L<<24))||steps[j-1].returned==44)&&j+1<stop&&steps[j+1].returned==181) && !(c->inherited && c->target==out) && !(j==c->first+1 && j+1==c->end-(c->contract?1:0) && s->d==64) && !(j==c->first+2 && steps[j-1].returned==237 && steps[j+1].returned==238 && j+2==c->end-(c->contract?1:0) && s->d==64)) || c->callee->np<0 || !native64typedconcatarg(s,fpbytes,mpbytes,c->callee->size,c->callee->map,c->callee->np))return 0;
                continue;
            }
            if((s->op!=IMOVB && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF && s->op!=IMOVP) ||
               s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || s->returned!=64 || sm==AXXX ||
               !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || s->d<64 || s->d>65535 ||
               (unsigned long)s->d>(unsigned int)c->callee->size || width>(unsigned int)c->callee->size-s->d || s->d%width)return 0;
        }
        if(c->contract){
            s=steps+c->end-1;width=native64wmtuplewidth(c->contract)?native64wmtuplewidth(c->contract):native64geometryresultwidth(c->contract)?native64geometryresultwidth(c->contract):c->contract==16?16:c->contract==72?24:c->contract==88?88:c->contract==48?16:c->contract&15;
            if(s->returned==60){if(c->contract!=24||!c->external||c->inherited||s->op!=ILEA||s->mode!=(SRC(AFP|AIND)|DST(AFP|AIND))||s->d!=32||!native64typedrootbindingfield(s->m)||!native64typedrootoperand(AFP,s->s,fpbytes,mpbytes))return 0;
            }else if(s->returned==85){if(!native64wmtuplewidth(c->contract)||c->inherited||s->op!=ILEA||s->mode!=(SRC(AFP)|DST(AFP|AIND))||s->m||s->d!=32||s->s<64||s->s%8||(unsigned long)s->s>fpbytes||width>fpbytes-s->s)return 0;
            }else if(s->returned==131){
                if(c->contract!=24 || c->inherited || s->op!=ILEA || s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->m || s->d!=32 || !native64typedrootoperand(AFP,s->s,fpbytes,mpbytes) || !c->first || steps[c->first-1].returned!=131 || steps[c->first-1].op!=IINDL || steps[c->first-1].s!=s->s)return 0;
            }else if(s->returned==81&&c->inherited){if(!c->external||c->contract!=96||c->callerresultcontract!=112||s->op!=ILEA||s->mode!=(SRC(AFP|AIND)|DST(AFP|AIND))||s->s!=32||s->m!=8||s->d!=32)return 0;
            }else if(s->returned==81){if(c->inherited||!native64geometryresultwidth(c->contract)||!native64windowgeometryfield(s->m,native64geometryresultwidth(c->contract))||s->op!=ILEA||s->mode!=(SRC(AFP|AIND)|DST(AFP|AIND))||s->d!=32||!native64typedrootoperand(AFP,s->s,fpbytes,mpbytes))return 0;
            }else if(s->returned==93){
                if(c->external||c->contract!=24||c->inherited||s->op!=ILEA||s->mode!=(SRC(AFP|AIND)|DST(AFP|AIND))||s->m!=24||s->d!=32||!native64typedrootoperand(AFP,s->s,fpbytes,mpbytes))return 0;
            }else if(s->returned==245){
                if(!c->external || c->contract!=4 || c->inherited || s->op!=ILEA || s->mode!=(SRC(AFP|AIND)|DST(AFP|AIND)) || s->m!=12 || s->d!=32 || !native64typedrootoperand(AFP,s->s,fpbytes,mpbytes))return 0;
            }else if(s->returned==220){
                if(!c->external || c->contract!=24 || c->inherited || s->op!=ILEA || s->mode!=(SRC(AFP|AIND)|DST(AFP|AIND)) || s->m!=24 || s->d!=32 || !native64typedrootoperand(AFP,s->s,fpbytes,mpbytes))return 0;
            }else{
                if(s->m || s->d!=32)return 0;
                if(!c->inherited && USRC(s->mode)==AMP){
                    if(native64geometryresultwidth(c->contract)){
                        if(!c->external || s->op!=ILEA || s->mode!=(SRC(AMP)|DST(AFP|AIND)) || s->returned!=(128|c->contract) || s->s<0 || s->s%4 || (unsigned long)s->s>mpbytes || width>mpbytes-s->s)return 0;
                    }else if(c->contract!=24 || s->op!=ILEA || s->mode!=(SRC(AMP)|DST(AFP|AIND)) || s->returned!=(128|24) || !native64typedrootoperand(AMP,s->s,fpbytes,mpbytes))return 0;
                }else if(s->mode!=(SRC(AFP)|DST(AFP|AIND)))return 0;
                else if(c->inherited){if(s->op!=IMOVL || s->s!=32 || s->returned!=64)return 0;}
                else if(s->op!=ILEA || s->returned!=(128|c->contract) || s->s<64 || (unsigned long)s->s>fpbytes || width>fpbytes-s->s || s->s%(native64geometryresultwidth(c->contract)||c->contract==12?4:(c->contract==16 || c->contract==48 || c->contract==72 || c->contract==88)?8:width))return 0;
            }
        }
        s=steps+c->end;
        if(c->external){
            if(s->op!=IMCALL || s->mode!=(SRC(AFP)|AXIMM|DST(c->modulemode)) || s->m!=c->ordinal || s->s!=40 || s->d!=c->moduleoffset || s->returned!=(c->inherited?(c->contract==96&&c->callerresultcontract==112?78:32):0))return 0;
        }else if(s->op==ISPAWN){
            if(c->contract || c->inherited || c->resultcontract || s->mode!=(SRC(AFP)|DST(AIMM)) || s->m || s->s!=40 || s->returned!=186)return 0;
        }else if(s->op!=ICALL || s->mode!=(SRC(AFP)|DST(AIMM)) || s->m || s->s!=40 || s->returned!=(c->inherited?(c->contract==24 && (c->callerresultcontract==80 || c->callerresultcontract==124)?78:32):0))return 0;
    }
    for(i=0;i<count;){
        if(k<n && i==calls[k].first){i=calls[k++].end+1;continue;}
        checkstep=steps[i];
        if(checkstep.returned==157 || checkstep.returned==158){unsigned int gap,j;int first=native64librarybytefirst(steps,count,i);
            if(first<0||!native64librarybytepair(steps,count,i))return 0;gap=(steps[first].m>>8)&255;
            if(gap!=11){for(j=first+1;j<=first+1+gap;j++)if(native64typedblocked(j,calls,n))return 0;}
            else for(j=0;j<n;j++)if(calls[j].first<=first+gap&&calls[j].end>=(unsigned int)first){if(calls[j].external||calls[j].contract!=4||calls[j].inherited||calls[j].first<=(unsigned int)first||calls[j].end>=(unsigned int)first+12)return 0;}
        }
        if(native64typedbranch(checkstep.op)){
            if(checkstep.d<0 || (unsigned long)checkstep.d>=count || native64typedblocked(checkstep.d,calls,n) || native64typedprotectedentry(steps,count,checkstep.d))return 0;
            if(!native64typedbranchprobe(&probe,&checkstep,fpbytes,mpbytes))return 0;
        }else if(!native64typedcode(&probe,1,&checkstep,1,fpbytes,mpbytes))return 0;
        i++;
    }
    k=0;
    for(i=0;i<count;){
        if(k<n && i==calls[k].first){
            c=calls+k++;
            for(j=i;j<=c->end;j++){
                s=steps+j;p=out+j;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;
                if(j==i){p->s.imm=c->external?c->moduleoffset:c->ordinal;p->reg=c->external?c->ordinal:0;p->d.ind=40;if(c->external&&(c->modulemode&AIND)){p->s.i.f=c->moduleoffset&65535;p->s.i.s=(c->moduleoffset>>16)&255;}}
                else if(j==c->end){p->s.ind=40;if(c->external){p->reg=c->ordinal;p->d.ind=c->moduleoffset;if(c->modulemode&AIND){p->d.i.f=c->moduleoffset&65535;p->d.i.s=(c->moduleoffset>>16)&255;}}else p->d.ins=c->target;}
                else if(s->op==IMOVM && (native64imagegeometryoffset(s->returned)!=~0U)){p->s.i.f=s->s;p->s.i.s=native64imagegeometryoffset(s->returned);p->reg=s->m;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==80||s->returned==Native64TypedBorrowIndex){native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else if(s->returned==65){native64rawusecode(p,s);}
                else if(s->returned==84){p->s.i.f=s->s;p->s.i.s=s->m>>8;p->reg=s->m&255;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==60){p->reg=(s->m>=256?8192:2048)|512|24;p->s.i.f=s->s;p->s.i.s=s->m>=256?s->m-256:s->m;p->d.i.f=40;p->d.i.s=32;}
                else if(s->returned==85){p->reg=c->contract;p->s.ind=s->s;p->d.i.f=40;p->d.i.s=32;}
                else if(s->returned==81){p->reg=(c->inherited?4096:512)|c->contract;p->s.i.f=s->s;p->s.i.s=s->m;p->d.i.f=40;p->d.i.s=32;}
                else if(s->returned==86){p->reg=s->m;p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==93){p->reg=512|24;p->s.i.f=s->s;p->s.i.s=88;p->d.i.f=40;p->d.i.s=32;}
                else if(s->returned==153||s->returned==154){native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else if(s->returned==156){if(USRC(s->mode)&AIND){p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;}else p->s.imm=s->s;p->reg=native64librarymiddle(s);p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==Native64TypedRecordArg)native64recordargcode(p,s);
                else if(s->returned==Native64TypedArrayArgIndex){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==155){if(USRC(s->mode)&AIND){p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;}else p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==131){p->s.ind=s->s;p->reg=1024|24;p->d.i.f=40;p->d.i.s=32;}
                else if(s->returned==143){if(s->op==IINDL){p->s.ind=s->s;p->reg=56;p->d.imm=s->d;}else{p->s.i.f=56;p->s.i.s=0;p->d.ind=s->d;}}
                else if(s->returned==127){if(s->op==IINDL){p->reg=s->m;p->s.ind=s->s;p->d.ind=s->d;}else{p->s.i.f=56;p->s.i.s=0;p->d.i.f=40;p->d.i.s=s->d;}}
                else if(s->returned==130 || s->returned==137 || (s->returned==152 && s->op==ICVTWF)){p->s.imm=s->s;p->reg=s->m;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==46){if(!native64typedcode(p,1,s,1,fpbytes,mpbytes))return 0;}
                else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else if(s->returned==252){if(s->op==IHEADP || s->op==ITAIL && UDST(s->mode)==(AFP|AIND)){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else if(s->returned==245){p->s.i.f=s->s;p->s.i.s=12;p->reg=512|4;p->d.i.f=40;p->d.i.s=32;}
                else if(s->returned==220){p->s.i.f=s->s;p->s.i.s=24;p->reg=512|24;p->d.i.f=40;p->d.i.s=32;}
                else if(s->returned==Native64TypedFieldArg || s->returned==233){p->s.i.f=s->s;p->s.i.s=s->m;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==237){native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else if(s->returned==238){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==187){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==182){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==74||s->returned==76){if(s->op==IINDL){p->s.ind=s->s;p->reg=48;p->d.imm=s->d;}else{p->s.i.f=48;p->s.i.s=0;p->reg=s->m;p->d.i.f=40;p->d.i.s=s->d;}}
                else if(s->returned==75){p->s.ind=s->s;p->reg=s->m;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==69||s->returned==71){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==70){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==67){p->reg=0;p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;p->d.i.f=40;p->d.i.s=s->d;}
                else if(s->returned==61||s->returned==66){native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else if(s->returned==44 || s->returned==63 || s->returned==87 || s->returned==180 || s->returned==181){checkstep=*s;if(s->returned!=63)checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==141){p->s.ind=s->s;p->reg=0;p->d.ind=s->d;}
                else if(s->returned==166 || s->returned==235){checkstep=*s;checkstep.returned=s->m==24?214:Native64TypedDraw;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==Native64TypedArgClear){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(s->returned==Native64TypedListPrep){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}
                else if(!s->returned){native64typedcode(p,1,s,1,fpbytes,mpbytes);}
                else{p->s.imm=s->s;p->d.i.f=40;p->d.i.s=s->d;if(s->op==IADDC)p->reg=s->m;else if(s->op==IMOVM||s->op==IHEADM)p->reg=s->m;if(c->contract && !c->inherited && j==c->end-1)p->reg=c->contract;}
            }
            i=c->end+1;continue;
        }
        s=steps+i;checkstep=*s;
        if(native64typedbranch(s->op)){native64typedbranchprobe(out+i,s,fpbytes,mpbytes);out[i].d.ins=out+s->d;}
        else native64typedcode(out+i,1,&checkstep,1,fpbytes,mpbytes);
        i++;
    }
    return 1;
}
#endif
