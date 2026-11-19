#ifndef NATIVE64_TYPED_MODULE_H
#define NATIVE64_TYPED_MODULE_H
#include "native64-typed-multi.h"
typedef struct Native64ModuleSite Native64ModuleSite;
typedef struct Native64ModuleFunction Native64ModuleFunction;
typedef struct Native64ModuleInit Native64ModuleInit;
typedef struct Native64ModuleSpec Native64ModuleSpec;
typedef struct Native64ModuleImport Native64ModuleImport;
typedef struct Native64ModuleTable Native64ModuleTable;
typedef struct Native64ModuleOwner Native64ModuleOwner;
typedef struct Native64ModuleCaseRow Native64ModuleCaseRow;
typedef struct Native64ModuleCase Native64ModuleCase;
struct Native64ModuleCaseRow {
    vlong low,high;unsigned int target; /* Inclusive numeric source bounds. */
    const uchar *start,*stop;unsigned int nstart,nstop; /* Explicit UTF-8 bytes; nil stop is a singleton. */
};
struct Native64ModuleCase {
    unsigned int pc,offset,op,count,fallback; /* Local function PCs, native MP base. */
    const Native64ModuleCaseRow *rows;
};
struct Native64ModuleOwner {Module *module;int rooted;Native64ModuleOwner *self;};
struct Native64ModuleSite {
    unsigned int first,end,target,contract,inherited;
    unsigned int external,table,modulemode,moduleoffset;
};
struct Native64ModuleImport {
    const char *name;unsigned int signature,bytes;
    const uchar *map;unsigned int mapbytes,resultcontract; /* Imported result: 72 three roots; 96/112 raw Point/Rect. */
};
struct Native64ModuleTable {const Native64ModuleImport *imports;unsigned int count;};
struct Native64ModuleFunction {
    const char *name;
    unsigned int signature,exported;
    const Native64TypedStep *code;
    unsigned int count,bytes;
    const uchar *map;
    unsigned int mapbytes;
    const Native64ModuleSite *sites;
    unsigned int nsites;
    const Native64ModuleCase *cases;unsigned int ncases;
    unsigned int resultcontract; /* Native-only 48: Module/List pair; 72: three channel roots. Zero keeps legacy contracts. */
};

struct Native64ModuleInit {unsigned int offset,width;uvlong bits;};
typedef struct Native64ModuleString Native64ModuleString;
struct Native64ModuleString {unsigned int offset,bytes;const uchar *text;};
struct Native64ModuleSpec {
    const char *name,*path;
    const Native64ModuleFunction *functions;
    unsigned int nfunctions,entry,databytes;
    const uchar *datamap;
    unsigned int mapbytes;
    const Native64ModuleInit *initial;
    unsigned int ninitial;
    const Native64ModuleTable *tables;unsigned int ntables;
    String *(*string)(char*,int); /* Trusted owner's real length-aware String constructor. */
    const Native64ModuleString *strings;unsigned int nstrings;
    Modlink *(*load)(Module*,WORD,char*); /* Trusted service; returns a pinned owned instance or nil/H. */
};
#include "native64-typed-tables.h"

static int
native64modulename(const char *s)
{
    unsigned int i;
    if(s==nil || s==H || !*s)return 0;
    for(i=0;i<=255;i++)if(!s[i])return 1;
    return 0;
}

static int
native64modulemap(unsigned int bytes,const uchar *map,unsigned int n,int frame)
{
    unsigned int i,b;
    if(bytes>1024*1024 || bytes%8 || (frame && bytes<64) || n!=(bytes+63)/64 || (n && (map==nil || map==H)) || (frame && map[0]))return 0;
    for(i=0;i<n;i++)for(b=0;b<8;b++)if((map[i]&(0x80>>b)) && i*64+b*8+8>bytes)return 0;
    return 1;
}

/* Compiler-proved List/String roles still require actual native root cells.
 * The transient result pointer is checked by the existing result binder. */
static int
native64modulelists(const Native64ModuleSpec *spec,const Native64ModuleFunction *fn)
{
 unsigned int i,k,bit,mode,root;long offset;const uchar *map;Inst probe;Native64TypedStep copy;const Native64TypedStep *step;
 for(i=0;i<fn->count;i++){
  step=fn->code+i;
  if(step->returned==213 && !native64typedwindowlist(step,fn->bytes,spec->databytes))return 0;
  if(step->op!=ICONSP && step->op!=IHEADP && step->op!=ITAIL && step->op!=ICONSW && step->op!=IHEADW)continue;
  copy=*step;if(copy.returned==Native64TypedListPrep || copy.returned==187 || copy.returned==213)copy.returned=0;
  if(!native64typedcode(&probe,1,&copy,1,fn->bytes,spec->databytes))return 0;
  for(k=0;k<2;k++){
   if(k && step->returned && step->returned!=213)continue;
   mode=k?UDST(step->mode):USRC(step->mode);offset=k?step->d:step->s;
   if(mode==AIMM)continue;root=step->op==ICONSW?k==1:step->op==IHEADW?k==0:1;
   map=mode==AFP?fn->map:spec->datamap;bit=offset/8;
   if(!!(map[bit/8] & (0x80>>(bit%8)))!=root)return 0;
  }
 }
 return 1;
}

static int
native64modulecontext(const Native64ModuleSpec*spec,const Native64ModuleFunction*fn)
{
 unsigned int i,k,mode,bit;long offsets[2];const uchar*map;const Native64TypedStep*step;
 for(i=0;i<fn->count;i++){
  step=fn->code+i;if(step->returned<Native64TypedContext || step->returned>=Native64TypedContextNil+3)continue;
  if(!native64typedcontextstep(step,fn->bytes,spec->databytes) || (step->returned!=Native64TypedContext && (step->d<0 || (unsigned long)step->d>=fn->count)))return 0;
  offsets[0]=step->s;offsets[1]=step->returned==Native64TypedContext?step->d:step->m;
  for(k=0;k<2;k++){
   mode=(k?(step->returned==Native64TypedContext?UDST(step->mode):AMP):USRC(step->mode))&~AIND;
   map=mode==AFP?fn->map:spec->datamap;bit=offsets[k]/8;
   if((map[bit/8] & (128>>(bit%8)))==0)return 0;
  }
  /* Field operations cannot be hidden inside a pending call setup. */
  for(k=0;k<fn->nsites;k++)if(fn->sites[k].first<=i && i<=fn->sites[k].end)return 0;
 }
 return 1;
}

static int
native64modulewindow(const Native64ModuleSpec*spec,const Native64ModuleFunction*fn)
{
 unsigned int i,k,mode,bit;long offsets[2];const uchar*map;const Native64TypedStep*step;
 for(i=0;i<fn->count;i++){
  step=fn->code+i;if(step->returned<Native64TypedWindow || step->returned>=Native64TypedWindowNil+6)continue;
  if(!native64typedwindowstep(step,fn->bytes,spec->databytes) || (step->returned!=Native64TypedWindow && (step->d<0 || (unsigned long)step->d>=fn->count)))return 0;
  offsets[0]=step->s;offsets[1]=step->returned==Native64TypedWindow?step->d:step->m;
  for(k=0;k<2;k++){
   mode=(k?(step->returned==Native64TypedWindow?UDST(step->mode):AMP):USRC(step->mode))&~AIND;
   map=mode==AFP?fn->map:spec->datamap;bit=offsets[k]/8;
   if(!(map[bit/8] & (128 >> (bit%8))))return 0;
  }
  for(k=0;k<fn->nsites;k++)if(fn->sites[k].first<=i && i<=fn->sites[k].end)return 0;
 }
 return 1;
}

static int
native64moduledrawfields(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int i,k,bit,mode,found,bytes,np;long offsets[2];const uchar*map;const Native64TypedStep*step;const Native64ModuleSite*site;const Native64ModuleImport*request;const Native64ModuleFunction*child;
 for(i=0;i<f->count;i++){
  step=f->code+i;
  if(step->returned==166 || step->returned==235){
   Native64TypedStep copy=*step;const Native64TypedStep*next;
   copy.returned=Native64TypedDraw;
   if(i+2>=f->count || step->m!=(step->returned==235?8:16) || UDST(step->mode)!=AFP || (USRC(step->mode)!=(AFP|AIND) && USRC(step->mode)!=(AMP|AIND)) || !native64typeddrawstep(&copy,f->bytes,s->databytes))return 0;
   next=f->code+i+1;if(next->returned<160 || next->returned>165 || next->op!=IMOVM || USRC(next->mode)!=(AFP|AIND) || next->s!=step->d || f->code[i+2].returned!=Native64TypedArgClear || f->code[i+2].d!=step->d)return 0;
   found=0;for(k=0;k<f->nsites;k++)if(f->sites[k].first<i && i+2<f->sites[k].end && f->sites[k].external==(step->returned==166))found++;if(found!=1)return 0;
   map=(USRC(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(0==(map[bit/8]&(128>>(bit%8))))return 0;bit=step->d/8;if(!(f->map[bit/8]&(128>>(bit%8))))return 0;continue;
  }
  if(step->returned==Native64TypedFieldArg || step->returned==233){
   found=0;
   for(k=0;k<f->nsites;k++)if(f->sites[k].first<i && i<f->sites[k].end){
    if(found++)return 0;site=f->sites+k;
    if((step->returned==233 && !site->external) || site->external>1 || site->end>=f->count || f->code[site->first].op!=(site->external?IMFRAME:IFRAME) || f->code[site->end].op!=(site->external?IMCALL:ICALL))return 0;
    if(site->external){if(site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;request=s->tables[site->table].imports+site->target;bytes=request->bytes;map=request->map;np=request->mapbytes;}
    else{if(site->target>=s->nfunctions)return 0;child=s->functions+site->target;if(!native64modulemap(child->bytes,child->map,child->mapbytes,1))return 0;bytes=child->bytes;map=child->map;np=child->mapbytes;}
    if(!native64typedfieldarg(step,f->bytes,s->databytes,bytes,map,np))return 0;
   }
   if(!found)return 0;mode=USRC(step->mode)&~AIND;map=mode==AFP?f->map:s->datamap;bit=step->s/8;if((map[bit/8]&(128 >> (bit%8)))==0)return 0;continue;
  }
  if(step->returned<Native64TypedDraw || step->returned>=Native64TypedDrawNil+11)continue;
  if(!native64typeddrawstep(step,f->bytes,s->databytes) || (step->returned!=Native64TypedDraw && (step->d<0 || (unsigned long)step->d>=f->count)))return 0;
  offsets[0]=step->s;offsets[1]=step->returned==Native64TypedDraw?step->d:step->m;
  for(k=0;k<2;k++){mode=(k?(step->returned==Native64TypedDraw?UDST(step->mode):AMP):USRC(step->mode))&~AIND;map=mode==AFP?f->map:s->datamap;bit=offsets[k]/8;if((map[bit/8]&(128 >> (bit%8)))==0)return 0;}
  for(k=0;k<f->nsites;k++)if(f->sites[k].first<=i && i<=f->sites[k].end)return 0;
 }
 return 1;
}

static int
native64modulereadonlynil(const Native64ModuleSpec*s,long offset)
{
 unsigned int i,j,n=0;
 if(s->nstrings>65536 || (s->nstrings && (s->strings==nil || s->strings==H)))return 0;
 for(i=0;i<s->nfunctions;i++){if(!s->functions[i].count || s->functions[i].count>65536-n || s->functions[i].code==nil || s->functions[i].code==H)return 0;n+=s->functions[i].count;}
 for(i=0;i<s->nstrings;i++)if(s->strings[i].offset==(unsigned long)offset)return 0;
 for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(s->functions[i].code[j].op==ILEA && USRC(s->functions[i].code[j].mode)==AMP && s->functions[i].code[j].s/8==offset/8)return 0;
 for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(UDST(s->functions[i].code[j].mode)==AMP && s->functions[i].code[j].d/8==offset/8)return 0;
 return 1;
}
static int
native64modulestringpreps(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int i,j,k,n,found,bit,mode;long slot;const uchar*map;const Native64TypedStep*step;const Native64ModuleSite*site=nil;const Native64ModuleImport*request;
 for(i=0;i<f->count;i++){
  step=f->code+i;if(step->returned!=180 && step->returned!=181)continue;
  if(step->returned!=180 || step->op!=ICVTWC || !native64typedstringprep(step,f->bytes,s->databytes))return 0;
  slot=step->d;bit=slot/8;if((f->map[bit/8]&(0x80>>(bit%8)))==0)return 0;
  mode=USRC(step->mode);if(mode!=AIMM){map=mode==AFP?f->map:s->datamap;bit=step->s/8;if(map[bit/8]&(0x80>>(bit%8)))return 0;}
  found=0;for(k=0;k<f->nsites;k++)if(f->sites[k].first<i && i<f->sites[k].end){if(found++)return 0;site=f->sites+k;}
  if(!found || site->external!=1 || site->end>=f->count || site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;
  request=s->tables[site->table].imports+site->target;j=i+1;n=0;
  while(j<site->end && f->code[j].returned==180){
   step=f->code+j;if(n++>=8 || step->op!=IADDC || step->d!=slot || !native64typedstringprep(step,f->bytes,s->databytes) || !((USRC(step->mode)==AFP && step->s==slot) || ((step->mode&ARM)==AXINF && step->m==slot)))return 0;
   for(k=0;k<2;k++){mode=k?((step->mode&ARM)==AXINF?AFP:AMP):USRC(step->mode);map=mode==AFP?f->map:s->datamap;bit=(k?step->m:step->s)/8;if((map[bit/8]&(0x80>>(bit%8)))==0)return 0;}j++;
  }
  if(j+1>=site->end)return 0;step=f->code+j;
  if(!native64typedconcatarg(step,f->bytes,s->databytes,request->bytes,request->map,request->mapbytes) || !((USRC(step->mode)==AFP && step->s==slot) || ((step->mode&ARM)==AXINF && step->m==slot)))return 0;
  for(k=0;k<2;k++){mode=k?((step->mode&ARM)==AXINF?AFP:AMP):USRC(step->mode);map=mode==AFP?f->map:s->datamap;bit=(k?step->m:step->s)/8;if((map[bit/8]&(0x80>>(bit%8)))==0)return 0;}
  step=f->code+j+1;if(step->returned!=181 || step->d!=slot || !native64typedstringprep(step,f->bytes,s->databytes) || !native64modulereadonlynil(s,step->s))return 0;bit=step->s/8;if((s->datamap[bit/8]&(0x80>>(bit%8)))==0)return 0;i=j+1;
 }return 1;
}

static int
native64modulelocalclears(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int i,k,found,bit;Native64TypedStep copy;const Native64TypedStep*step,*before;const Native64ModuleFunction*child;const Native64ModuleSite*site;
 for(i=0;i<f->count;i++){
  step=f->code+i;if(step->returned!=182)continue;copy=*step;copy.returned=Native64TypedListPrep;
  if(!i || step->op!=IMOVP || !native64typedlistprep(&copy,f->bytes,s->databytes) || !native64modulereadonlynil(s,step->s))return 0;
  before=f->code+i-1;if(before->op!=IMOVP || before->returned!=64 || before->mode!=(SRC(AFP)|DST(AFP|AIND)) || before->m || before->s!=step->d)return 0;
  found=0;for(k=0;k<f->nsites;k++)if(f->sites[k].first<i && i<f->sites[k].end){
   if(found++)return 0;site=f->sites+k;if(site->external || site->target>=s->nfunctions)return 0;child=s->functions+site->target;
   if(!native64modulemap(child->bytes,child->map,child->mapbytes,1) || before->d<64 || before->d%8 || (unsigned long)before->d>child->bytes || 8>child->bytes-before->d)return 0;bit=before->d/8;if((child->map[bit/8]&(0x80>>(bit%8)))==0)return 0;
  }
  if(!found)return 0;bit=step->d/8;if((f->map[bit/8]&(0x80>>(bit%8)))==0)return 0;bit=step->s/8;if((s->datamap[bit/8]&(0x80>>(bit%8)))==0)return 0;
 }return 1;
}

static int
native64moduleargclears(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int i,k,found,bit;Native64TypedStep copy;const Native64TypedStep*step,*before;const Native64ModuleSite*site;const Native64ModuleImport*request;
 for(i=0;i<f->count;i++){
  step=f->code+i;if(step->returned!=Native64TypedArgClear)continue;
  copy=*step;copy.returned=Native64TypedListPrep;
  if(!i || step->op!=IMOVP || !native64typedlistprep(&copy,f->bytes,s->databytes) || !native64modulereadonlynil(s,step->s))return 0;
  before=f->code+i-1;
  if(before->s!=step->d || !((before->op==IMOVP && before->returned==64 && before->mode==(SRC(AFP)|DST(AFP|AIND)) && !before->m) ||
     (before->op==IMOVM && before->returned>=160 && before->returned<=165 && USRC(before->mode)==(AFP|AIND))))return 0;
  found=0;for(k=0;k<f->nsites;k++)if(f->sites[k].first<i && i<f->sites[k].end){
   if(found++)return 0;site=f->sites+k;
   if(!site->external){
    const Native64ModuleFunction*child;
    if(i<2 || f->code[i-2].returned!=235 || before->op!=IMOVM || site->target>=s->nfunctions)return 0;
    child=s->functions+site->target;if(!native64modulemap(child->bytes,child->map,child->mapbytes,1) || !native64typedgeometryarg(before,f->bytes,s->databytes,child->bytes,child->map,child->mapbytes))return 0;
    continue;
   }
   if(site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;
   request=s->tables[site->table].imports+site->target;
   if(before->op==IMOVM){if(!native64typedgeometryarg(before,f->bytes,s->databytes,request->bytes,request->map,request->mapbytes))return 0;}
   else{
    if(before->d<64 || before->d%8 || (unsigned long)before->d>request->bytes || 8>request->bytes-before->d)return 0;
    bit=before->d/8;if((request->map[bit/8] & (128 >> (bit%8)))==0)return 0;
   }
  }
  if(!found)return 0;bit=step->d/8;if(!(f->map[bit/8]&(128 >> (bit%8))))return 0;bit=step->s/8;if(!(s->datamap[bit/8]&(128 >> (bit%8))))return 0;
 }
 return 1;
}

static int
native64modulelistprefixes(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int i,j,k,n,found,bit,mode;long slot;const Native64ModuleSite*site=nil;const Native64ModuleImport*request;const Native64TypedStep*step;const uchar*map;
 for(i=0;i<f->count;i++){
  step=f->code+i;if(step->returned!=187)continue;
  if(step->op!=IMOVP || !native64typedlistprefix(step,f->bytes,s->databytes))return 0;
  slot=step->d;found=0;
  for(k=0;k<f->nsites;k++)if(f->sites[k].first<i && i<f->sites[k].end){if(found++)return 0;site=f->sites+k;}
  if(!found || !site->external || site->end>=f->count || f->code[site->end].op!=IMCALL || site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;
  request=s->tables[site->table].imports+site->target;n=0;
  for(j=i;j<site->end && f->code[j].returned==187;j++){
   step=f->code+j;
   if(!native64typedlistprefix(step,f->bytes,s->databytes) || step->d!=slot || (j==i?step->op!=IMOVP:step->op!=ICONSP) || j-i>256)return 0;
   for(k=0;k<2;k++){
    mode=k?AFP:USRC(step->mode);map=mode==AFP?f->map:s->datamap;bit=(k?step->d:step->s)/8;
    if((map[bit/8] & (128 >> (bit%8)))==0)return 0;
   }
   if(j!=i)n++;
  }
  if(!n || j>=site->end)return 0;step=f->code+j;
  if(step->op!=IMOVP || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->returned!=64 || step->m || step->s!=slot || step->d<64 || step->d%8 ||
     (unsigned long)step->d>request->bytes || 8>request->bytes-step->d)return 0;
  bit=step->d/8;if(!(request->map[bit/8]&(128>>(bit%8))))return 0;
  if(j+1>=site->end || f->code[j+1].op!=IMOVP || f->code[j+1].mode!=(SRC(AMP)|DST(AFP)) || f->code[j+1].returned!=187 ||
     !native64modulereadonlynil(s,f->code[j+1].s) || f->code[j+1].d!=slot || !native64typedlistprefix(f->code+j+1,f->bytes,s->databytes))return 0;
  bit=f->code[j+1].s/8;if((s->datamap[bit/8]&(128>>(bit%8)))==0)return 0;
  i=j+1;
 }
 return 1;
}

static int
native64modulelistpreps(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int i,j,k,n,found,bit,mode;long slot,nilslot=-1;const Native64ModuleSite*site=nil;const Native64ModuleImport*request;const Native64TypedStep*step;const uchar*map;
 for(i=0;i<f->count;i++){
  step=f->code+i;if(step->returned!=Native64TypedListPrep)continue;
  if(step->op!=IMOVP || !native64typedlistprep(step,f->bytes,s->databytes))return 0;
  if(nilslot>=0 && nilslot!=step->s)return 0;
  if(nilslot<0){if(!native64modulereadonlynil(s,step->s))return 0;nilslot=step->s;}
  slot=step->d;found=0;
  for(k=0;k<f->nsites;k++)if(f->sites[k].first<i && i<f->sites[k].end){if(found++)return 0;site=f->sites+k;}
  if(!found || !site->external || site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;
  request=s->tables[site->table].imports+site->target;n=0;
  for(j=i;j<site->end && f->code[j].returned==Native64TypedListPrep;j++){
   step=f->code+j;
   if(!native64typedlistprep(step,f->bytes,s->databytes) || step->d!=slot || (j==i?step->op!=IMOVP:step->op!=ICONSP) || j-i>256)return 0;
   for(k=0;k<2;k++){
    mode=k?AFP:USRC(step->mode);map=mode==AFP?f->map:s->datamap;bit=(k?step->d:step->s)/8;
    if((map[bit/8] & (128 >> (bit%8)))==0)return 0;
   }
   if(j!=i)n++;
  }
  if(!n || j>=site->end)return 0;step=f->code+j;
  if(step->op!=IMOVP || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->returned!=64 || step->m || step->s!=slot || step->d<64 || step->d%8 ||
     (unsigned long)step->d>request->bytes || 8>request->bytes-step->d)return 0;
  bit=step->d/8;if(!(request->map[bit/8]&(128>>(bit%8))))return 0;
  if(j+1>=site->end || f->code[j+1].op!=IMOVP || f->code[j+1].returned!=Native64TypedListPrep ||
     f->code[j+1].s!=f->code[i].s || f->code[j+1].d!=slot || !native64typedlistprep(f->code+j+1,f->bytes,s->databytes))return 0;
  i=j+1;
 }
 return 1;
}

static int
native64modulegeometry(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int i,k,j,found,bit;const Native64TypedStep*step;const Native64ModuleSite*site;const Native64ModuleImport*request;const uchar*map;
 for(i=0;i<f->count;i++){
  step=f->code+i;if((step->returned>=208 && step->returned<=212)||step->returned==215)continue;if(step->op!=IMOVM && !(step->returned==64 && (step->mode&ARM)==AXIMM && UDST(step->mode)==(AFP|AIND)) && native64imagegeometryoffset(step->returned)==~0U)continue;
  found=0;
  for(k=0;k<f->nsites;k++){
   site=f->sites+k;if(i<=site->first || i>=site->end)continue;
   if(found++)return 0;
   if(site->external){
    if(site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;
    request=s->tables[site->table].imports+site->target;
    if(!native64typedgeometryarg(step,f->bytes,s->databytes,request->bytes,request->map,request->mapbytes))return 0;
   }else{
    const Native64ModuleFunction*child;
    if(step->returned==227 || step->returned==230 || site->target>=s->nfunctions || site->first>=f->count || f->code[site->first].op!=IFRAME || site->end>=f->count || f->code[site->end].op!=ICALL)return 0;
    child=s->functions+site->target;
    if(!native64modulemap(child->bytes,child->map,child->mapbytes,1) || !native64typedgeometryarg(step,f->bytes,s->databytes,child->bytes,child->map,child->mapbytes))return 0;
   }
  }
  if(!found)return 0;
  map=(USRC(step->mode)&~AIND)==AFP?f->map:s->datamap;
  if(native64imagegeometryoffset(step->returned)!=~0U){bit=step->s/8;if(0==(map[bit/8]&(128>>(bit%8))))return 0;continue;}
  for(j=0;j<(unsigned long)step->m;j+=4){bit=(step->s+j)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
 }
 return 1;
}

/* Immutable readable checked-compiler metadata, not untrusted file inputs. */
#include "native64-typed-alt.h"

static int
native64modulecheck(const Native64ModuleSpec *s,unsigned int *instructions,unsigned int *exports,unsigned int *scratch,unsigned int *imports)
{
    unsigned int i,j,code=0,links=0,maxsites=0,total,end=0,at,bit,nimports=0,bytes;const Native64ModuleFunction *f;const Native64ModuleInit *v;
    const Native64ModuleImport *request;const Native64ModuleTable *table;const Native64ModuleSite *site;const uchar *map;const Native64TypedStep *step;
    if(!native64model() || s==nil || !native64modulename(s->name) || !native64modulename(s->path) ||
       !s->nfunctions || s->nfunctions>1024 || (s->entry!=~0U && s->entry>=s->nfunctions) || s->functions==nil ||
       !native64modulemap(s->databytes,s->datamap,s->mapbytes,0) || s->ninitial>65536 || (s->ninitial && s->initial==nil) || s->ntables>64 || (s->ntables && s->tables==nil))return 0;
    total=s->databytes;
    for(i=0;i<s->ntables;i++){
        table=s->tables+i;
        if(!table->count || table->count>4096-nimports || table->imports==nil)return 0;
        nimports+=table->count;
        for(j=0;j<table->count;j++){
            request=table->imports+j;
            if((request->resultcontract && request->resultcontract!=72 && !native64geometryresultwidth(request->resultcontract)) || !native64modulename(request->name) || !native64modulemap(request->bytes,request->map,request->mapbytes,1) ||
               request->bytes>65536 || request->bytes>32*1024*1024-total)return 0;
            total+=request->bytes;
        }
    }
    for(i=0;i<s->nfunctions;i++){
        f=s->functions+i;
        if(!native64modulename(f->name) || f->exported>1 || !f->count || f->count>65536-code || f->code==nil ||
           !native64modulemap(f->bytes,f->map,f->mapbytes,1) || f->bytes>32*1024*1024-total ||
           f->nsites>f->count/2 || (f->nsites && f->sites==nil))return 0;
        for(j=0;j<i;j++)if(!strcmp(f->name,s->functions[j].name))return 0;
        if(!native64modulealts(s,f))return 0;
        if(!native64modulelists(s,f) || !native64modulecontext(s,f) || !native64modulewindow(s,f) || !native64moduledrawfields(s,f) || !native64moduleargclears(s,f) || !native64modulestringpreps(s,f) || !native64modulelocalclears(s,f) || !native64modulelistprefixes(s,f) || !native64modulelistpreps(s,f) || !native64modulegeometry(s,f))return 0;
        for(j=0;j<f->count;j++)if((f->code[j].op==ILEA && USRC(f->code[j].mode)==AMP && f->code[j].returned!=190 && f->code[j].returned!=195 && f->code[j].returned!=197 && f->code[j].returned!=199 && f->code[j].returned!=203) || (f->code[j].returned==(128|24) && USRC(f->code[j].mode)!=AFP)){
            unsigned int k,found=0;step=f->code+j;
            if(step->op!=ILEA || step->mode!=(SRC(AMP)|DST(AFP|AIND)) || step->returned!=(128|24) || step->m || step->d!=32 || !native64typedrootoperand(AMP,step->s,f->bytes,s->databytes))return 0;
            bit=step->s/8;if(!(s->datamap[bit/8]&(128>>(bit%8))))return 0;
            for(k=0;k<f->nsites;k++)if(f->sites[k].end==j+1 && f->sites[k].first<j && f->sites[k].contract==24 && !f->sites[k].inherited)found++;
            if(found!=1)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==IEXIT){
            Inst probe;step=f->code+j;
            if(!native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        if(f->resultcontract && f->resultcontract!=48 && f->resultcontract!=72)return 0;
        for(j=0;j<f->nsites;j++){
            unsigned int k,r,modes[2],offsets[2];site=f->sites+j;
            if(site->first>=f->count || site->end>=f->count || site->end<=site->first)return 0;
            for(k=site->first+1;k<site->end;k++)if(!f->code[k].returned){
                step=f->code+k;if(site->external || !native64typedargprep(step,f->bytes,s->databytes))return 0;
                if(step->op!=IMOVP)continue;
                modes[0]=USRC(step->mode);modes[1]=AFP;offsets[0]=step->s;offsets[1]=step->d;
                for(r=0;r<2;r++){map=modes[r]==AFP?f->map:s->datamap;bit=offsets[r]/8;if(!(map[bit/8]&(0x80>>(bit%8))))return 0;}
            }
        }
        for(j=0;j<f->count;j++)if((f->code[j].op==ILENA || f->code[j].op==ICVTAC) && f->code[j].returned==64){
            unsigned int k,found=0,bit;step=f->code+j;
            for(k=0;k<f->nsites;k++){
                site=f->sites+k;
                if(site->first<j && j<site->end && (!site->contract || j<site->end-1)){
                    if(found++ || ((step->op==ILENA)!=site->external))return 0;
                    if(site->external){
                        if(site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;
                        request=s->tables[site->table].imports+site->target;
                        if(!native64typedcomputedarg(step,f->bytes,s->databytes,request->bytes,request->map,request->mapbytes))return 0;
                    }else{
                        const Native64ModuleFunction*child;
                        if(site->target>=s->nfunctions)return 0;child=s->functions+site->target;
                        if(!native64modulemap(child->bytes,child->map,child->mapbytes,1) || !native64typedcomputedarg(step,f->bytes,s->databytes,child->bytes,child->map,child->mapbytes))return 0;
                    }
                }
            }
            if(!found)return 0;
            map=USRC(step->mode)==AFP?f->map:s->datamap;bit=step->s/8;
            if(!(map[bit/8]&(0x80>>(bit%8))))return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==IADDC && f->code[j].returned==64){
            unsigned int k,found=0,modes[2],offsets[2];
            step=f->code+j;
            for(k=0;k<f->nsites;k++){
                site=f->sites+k;
                if(site->first<j && site->end>j && (!site->contract || j<site->end-1)){
                    if(found++)return 0;
                    if(site->external){
                        if(site->table>=s->ntables || site->target>=s->tables[site->table].count)return 0;
                        request=s->tables[site->table].imports+site->target;
                        if(!native64typedconcatarg(step,f->bytes,s->databytes,request->bytes,request->map,request->mapbytes))return 0;
                    }else{
                        const Native64ModuleFunction*child;
                        if(site->target>=s->nfunctions)return 0;child=s->functions+site->target;
                        if(!(site->target==i && site->inherited && site->end<f->count-1 && f->code[site->end+1].op==IRET) &&
                           !(j==site->first+1 && j+1==site->end-(site->contract?1:0) && step->d==64))return 0;
                        if(!native64modulemap(child->bytes,child->map,child->mapbytes,1) || !native64typedconcatarg(step,f->bytes,s->databytes,child->bytes,child->map,child->mapbytes))return 0;
                    }
                }
            }
            if(!found)return 0;
            modes[0]=USRC(step->mode);modes[1]=(step->mode&ARM)==AXINF?AFP:AMP;
            offsets[0]=step->s;offsets[1]=step->m;
            for(k=0;k<2;k++){
                map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;
                if(!(map[bit/8]&(0x80>>(bit%8))))return 0;
            }
        }
        for(j=0;j<f->count;j++)if((f->code[j].op==IADDC && f->code[j].returned!=64 && f->code[j].returned!=180) || f->code[j].op==IINDC || f->code[j].op==ISLICEC){
            unsigned int k,modes[3],offsets[3],roots[3];
            step=f->code+j;
            if(!native64typedstringstep(step,f->bytes,s->databytes))return 0;
            modes[0]=USRC(step->mode);modes[1]=(step->mode&ARM)==AXIMM?AIMM:(step->mode&ARM)==AXINF?AFP:AMP;modes[2]=UDST(step->mode);
            offsets[0]=step->s;offsets[1]=step->m;offsets[2]=step->d;
            roots[0]=step->op!=ISLICEC;roots[1]=step->op==IADDC;roots[2]=step->op!=IINDC;
            if(step->returned)modes[2]=AIMM;
            for(k=0;k<3;k++)if(modes[k]!=AIMM){
                map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;
                if(!!(map[bit/8]&(0x80>>(bit%8)))!=roots[k])return 0;
            }
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==214){
            unsigned int k,modes[2],offsets[2];step=f->code+j;if(!native64typedwmsrvwindow(step,f->bytes,s->databytes))return 0;
            modes[0]=USRC(step->mode)&~AIND;modes[1]=UDST(step->mode)&~AIND;offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++){map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==213)for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        for(j=0;j<f->count;j++)if((f->code[j].returned>=208 && f->code[j].returned<=212)||f->code[j].returned==215){
            unsigned int k,modes[2],offsets[2],width;step=f->code+j;if(!native64typedlocalgeometry(step,f->bytes,s->databytes))return 0;width=step->m&255;
            modes[0]=USRC(step->mode);modes[1]=UDST(step->mode);offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++){map=(modes[k]&~AIND)==AFP?f->map:s->datamap;if(modes[k]&AIND){bit=offsets[k]/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}else for(at=0;at<width;at++){bit=(offsets[k]+at)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==228 || f->code[j].returned==229){
            step=f->code+j;if(!native64typedpointerword(step,f->bytes,s->databytes) || (step->returned==229 && (step->d<0 || (unsigned long)step->d>=f->count)))return 0;
            map=(USRC(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
            if(step->returned==228){map=UDST(step->mode)==AFP?f->map:s->datamap;for(at=0;at<4;at++){bit=(step->d+at)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==231 || ((f->code[j].op==IBEQL || f->code[j].op==IBNEL) && (USRC(f->code[j].mode)==AFP || USRC(f->code[j].mode)==AMP) && f->code[j].returned)){
            unsigned int k,modes[2],offsets[2];step=f->code+j;if(!native64typedclientcompare(step,f->bytes,s->databytes) || step->d<0 || (unsigned long)step->d>=f->count)return 0;
            modes[0]=USRC(step->mode);modes[1]=(step->mode&ARM)==AXINF?AFP:AMP;offsets[0]=step->s;offsets[1]=step->m;
            for(k=0;k<2;k++){map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==232 || ((f->code[j].op==IBEQC || f->code[j].op==IBNEC) && (USRC(f->code[j].mode)&AIND) && f->code[j].returned)){
            unsigned int k,modes[2],offsets[2];step=f->code+j;if(!native64typedcursorcompare(step,f->bytes,s->databytes) || step->d<0 || (unsigned long)step->d>=f->count)return 0;
            modes[0]=USRC(step->mode)&~AIND;modes[1]=(step->mode&ARM)==AXINF?AFP:AMP;offsets[0]=step->s;offsets[1]=step->m;
            for(k=0;k<2;k++){map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==234 || (f->code[j].op==IANDW && (USRC(f->code[j].mode)&AIND) && f->code[j].returned)){
            step=f->code+j;if(!native64typedflagsand(step,f->bytes,s->databytes))return 0;
            map=(USRC(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
            map=UDST(step->mode)==AFP?f->map:s->datamap;for(at=0;at<4;at++){bit=(step->d+at)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==236 || (f->code[j].op==IORW && (UDST(f->code[j].mode)&AIND) && f->code[j].returned)){
            step=f->code+j;if(!native64typedflagsor(step,f->bytes,s->databytes))return 0;
            map=(UDST(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->d/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==207 || ((f->code[j].op==IBEQL || f->code[j].op==IBNEL) && (USRC(f->code[j].mode)&AIND) && f->code[j].m>=0x1000000L)){
            unsigned int k,modes[2],offsets[2];step=f->code+j;if(!native64typedfieldreference(step,f->bytes,s->databytes) || step->d<0 || (unsigned long)step->d>=f->count)return 0;
            modes[0]=USRC(step->mode)&~AIND;modes[1]=(step->mode&ARM)==AXINF?AFP:AMP;offsets[0]=step->s;offsets[1]=step->m&65535;
            for(k=0;k<2;k++){map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==205 || f->code[j].returned==206){
            unsigned int k,modes[2],offsets[2],root;step=f->code+j;if(!native64typedclientfield(step,f->bytes,s->databytes))return 0;
            modes[0]=USRC(step->mode);modes[1]=UDST(step->mode);offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++)if(modes[k]!=AIMM){root=!!(modes[k]&AIND) || step->returned==205;map=(modes[k]&~AIND)==AFP?f->map:s->datamap;bit=offsets[k]/8;if(!!(map[bit/8]&(128>>(bit%8)))!=root)return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==ISEND || f->code[j].op==IRECV || f->code[j].returned==188 || f->code[j].returned==204){
            unsigned int k,modes[2],offsets[2];step=f->code+j;if(!native64typedtransfer(step,f->bytes,s->databytes))return 0;
            modes[0]=USRC(step->mode);modes[1]=UDST(step->mode);offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++)if(modes[k]!=AIMM){map=(modes[k]&~AIND)==AFP?f->map:s->datamap;bit=offsets[k]/8;if(!!(map[bit/8]&(128>>(bit%8)))!=((step->op==ISEND?k==1:k==0)||(step->m&255)==24))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==INEWCMP || f->code[j].returned==183 || f->code[j].returned==185){
            unsigned int mode;step=f->code+j;if(!native64typedmappedchannel(step,f->bytes,s->databytes) || (step->returned==185 && f->resultcontract))return 0;
            if(step->returned==183){map=UDST(step->mode)==AFP?f->map:s->datamap;bit=step->d/8;if(0==(map[bit/8]&(128>>(bit%8))))return 0;}
            mode=(step->mode&ARM)==AXINF?AFP:(step->mode&ARM)==AXINM?AMP:AIMM;
            if(mode!=AIMM){map=mode==AFP?f->map:s->datamap;bit=step->m/8;if((map[bit/8]&(128>>(bit%8)))!=0)return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(native64typedchannelop(f->code[j].op)){
            unsigned int mode;step=f->code+j;if(!native64typedchannel(step,f->bytes,s->databytes) || (step->returned && f->resultcontract))return 0;
            if(!step->returned){map=UDST(step->mode)==AFP?f->map:s->datamap;bit=step->d/8;if((map[bit/8]&(128>>(bit%8)))==0)return 0;}
            mode=(step->mode&ARM)==AXINF?AFP:(step->mode&ARM)==AXINM?AMP:AIMM;
            if(mode!=AIMM){map=mode==AFP?f->map:s->datamap;bit=step->m/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==ICVTCW){
            Inst probe;unsigned int mode,k;step=f->code+j;
            if(!native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
            mode=USRC(step->mode);map=mode==AFP?f->map:s->datamap;bit=step->s/8;
            if(!(map[bit/8]&(128>>(bit%8))))return 0;
            for(k=0;k<4;k++){bit=(step->d+k)/8;if(f->map[bit/8]&(128>>(bit%8)))return 0;}
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==ICVTWC && f->code[j].returned!=180){
            Inst probe;unsigned int mode;step=f->code+j;
            if(!native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
            bit=step->d/8;if(!(f->map[bit/8]&(128>>(bit%8))))return 0;
            mode=USRC(step->mode);if(mode!=AIMM){map=mode==AFP?f->map:s->datamap;bit=step->s/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==IBEQC || f->code[j].op==IBNEC || f->code[j].op==IBLTC || f->code[j].op==IBLEC || f->code[j].op==IBGTC || f->code[j].op==IBGEC){
            unsigned int k,modes[2],offsets[2];Inst probe;step=f->code+j;
            if(step->returned==232)continue; /* Owner and String maps proved above. */
            {Native64TypedStep local=*step;
             if(step->d<0 || (unsigned long)step->d>=f->count)return 0;
             local.d=0;if(!native64typedcode(&probe,1,&local,1,f->bytes,s->databytes))return 0;
            }
            modes[0]=USRC(step->mode);modes[1]=(step->mode&ARM)==AXINF?AFP:AMP;offsets[0]=step->s;offsets[1]=step->m;
            for(k=0;k<2;k++){map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==INEWAZ || f->code[j].op==ISLICEA || (f->code[j].op==ICVTAC && f->code[j].returned!=64)){
            unsigned int k,modes[3],offsets[3],roots[3];step=f->code+j;
            if(!native64typedbytestep(step,f->bytes,s->databytes))return 0;
            modes[0]=USRC(step->mode);offsets[0]=step->s;roots[0]=step->op==ICVTAC;
            modes[1]=step->returned?AIMM:AFP;offsets[1]=step->d;roots[1]=1;
            modes[2]=step->op==ISLICEA && (step->mode&ARM)==AXINF?AFP:AIMM;offsets[2]=step->m;roots[2]=0;
            for(k=0;k<3;k++)if(modes[k]!=AIMM){
                map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;
                if(roots[k]!=!!(map[bit/8]&(0x80>>(bit%8))))return 0;
            }
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==IRAISE){
            step=f->code+j;at=USRC(step->mode);
            if(UDST(step->mode)!=AXXX || (step->mode&ARM)!=AXNON || step->m || step->d || step->returned ||
               !native64typedrootoperand(at,step->s,f->bytes,s->databytes))return 0;
            map=at==AFP?f->map:s->datamap;bit=step->s/8;
            if(!(map[bit/8]&(0x80>>(bit%8))))return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==173){
            unsigned int k;step=f->code+j;if(!native64typedcontextcopy(step,f->bytes,s->databytes))return 0;
            for(k=0;k<2;k++){map=((k?UDST(step->mode):USRC(step->mode))&~AIND)==AFP?f->map:s->datamap;bit=(k?step->d:step->s)/8;if((map[bit/8]&(128>>(bit%8)))==0)return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==172 || f->code[j].returned==174){
            step=f->code+j;if(!native64typedcontextnew(step,f->bytes))return 0;
            if(step->returned==172){bit=step->d/8;if((f->map[bit/8]&(128>>(bit%8)))==0)return 0;}else if(f->resultcontract)return 0;
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==194){
            Inst probe;unsigned int k,modes[2],offsets[2];step=f->code+j;
            if(!native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
            modes[0]=USRC(step->mode);modes[1]=UDST(step->mode);offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++)for(at=offsets[k];at<offsets[k]+16;at+=8){map=modes[k]==AFP?f->map:s->datamap;bit=at/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==196){
            Inst probe;unsigned int k,modes[2],offsets[2];step=f->code+j;
            if(!native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
            modes[0]=USRC(step->mode);modes[1]=UDST(step->mode);offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++)for(at=offsets[k];at<offsets[k]+24;at+=8){map=modes[k]==AFP?f->map:s->datamap;bit=at/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==201 || f->code[j].returned==202){
            Inst probe;unsigned int k,modes[2],offsets[2],contract,width;step=f->code+j;
            if(!native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
            contract=step->returned==201?104:120;width=native64fileiowidth(contract);
            modes[0]=USRC(step->mode);modes[1]=UDST(step->mode);offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++)for(at=0;at<width;at++){map=modes[k]==AFP?f->map:s->datamap;bit=(offsets[k]+at)/8;if(!!(map[bit/8]&(128>>(bit%8)))!=native64fileioroot(contract,at))return 0;}
            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==171){
            step=f->code+j;if(f->resultcontract!=72 || !native64typedtriplecopy(step,f->bytes))return 0;
            for(at=step->s;at<(unsigned long)step->s+24;at+=8){bit=at/8;if((f->map[bit/8]&(128>>(bit%8)))==0)return 0;}
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned>=168 && f->code[j].returned<=170){
            step=f->code+j;Inst probe;
            if(f->resultcontract!=72 || !native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
            map=USRC(step->mode)==AFP?f->map:s->datamap;bit=step->s/8;if((map[bit/8]&(128>>(bit%8)))==0)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==40 || f->code[j].returned==56){
            if(f->resultcontract!=48 || f->code[j].op!=IMOVP)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==ILOAD){
            unsigned int modes[2],offsets[2],k;
            step=f->code+j;
            if(s->load==nil || s->load==(Modlink*(*)(Module*,WORD,char*))H || step->m<0 || (unsigned long)step->m>=s->ntables ||
               (step->mode&ARM)!=AXIMM || step->returned)return 0;
            modes[0]=USRC(step->mode);modes[1]=UDST(step->mode);
            if(step->s<0 || step->d<0 || (unsigned long)step->s>1024*1024 || (unsigned long)step->d>1024*1024)return 0;
            offsets[0]=step->s;offsets[1]=step->d;
            for(k=0;k<2;k++){
                if(modes[k]!=AFP && modes[k]!=AMP)return 0;
                bytes=modes[k]==AFP?f->bytes:s->databytes;map=modes[k]==AFP?f->map:s->datamap;at=offsets[k];
                if(at%8 || at>bytes || 8>bytes-at || (modes[k]==AFP && at<64))return 0;
                bit=at/8;if((map[bit/8] & (0x80>>(bit%8)))==0)return 0;
            }
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==(128|72)){
            unsigned int k,found=0;for(k=0;k<f->nsites;k++)if(f->sites[k].end==j+1 && f->sites[k].contract==72 && !f->sites[k].inherited)found++;if(found!=1)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==(128|88)){
            unsigned int k,found=0;
            for(k=0;k<f->nsites;k++)if(f->sites[k].end==j+1 && f->sites[k].contract==88)found++;
            if(found!=1)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==220){
            unsigned int k,found=0;step=f->code+j;
            if(step->op!=ILEA || step->mode!=(SRC(AFP|AIND)|DST(AFP|AIND)) || step->m!=24 || step->d!=32 || !native64typedrootoperand(AFP,step->s,f->bytes,s->databytes))return 0;
            bit=step->s/8;if(!(f->map[bit/8]&(128>>(bit%8))))return 0;
            for(k=0;k<f->nsites;k++)if(f->sites[k].end==j+1 && f->sites[k].external && f->sites[k].contract==24 && !f->sites[k].inherited)found++;
            if(found!=1)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].returned==224 || f->code[j].returned==240){
            unsigned int k,found=0;for(k=0;k<f->nsites;k++)if(f->sites[k].end==j+1 && (128|f->sites[k].contract)==f->code[j].returned && !f->sites[k].inherited)found++;if(found!=1)return 0;
        }
        for(j=0;j<f->count;j++)if(f->code[j].op==ISPAWN || f->code[j].returned==186){
            unsigned int k,found=0,a;const Native64ModuleFunction*child;const Native64TypedStep*body;
            step=f->code+j;if(step->op!=ISPAWN || step->returned!=186 || step->mode!=(SRC(AFP)|DST(AIMM)) || step->m || step->s!=40)return 0;
            for(k=0;k<f->nsites;k++)if(f->sites[k].end==j){
                site=f->sites+k;if(found++ || site->external || site->contract || site->inherited || site->target>=s->nfunctions)return 0;
                child=s->functions+site->target;if(child->resultcontract || !child->code || child->count>65536)return 0;
                if(!native64modulemap(child->bytes,child->map,child->mapbytes,1))return 0;
                for(a=site->first+1;a<j;a++){
                    unsigned int width,root,mode,extent;body=f->code+a;if(body->returned==182)continue;
                    root=body->op==IMOVP;width=body->op==IMOVB?1:body->op==IMOVW?4:8;mode=USRC(body->mode);
                    if(body->returned!=64 || (body->op!=IMOVB && body->op!=IMOVW && body->op!=IMOVL && body->op!=IMOVF && !root) || body->mode!=(SRC(mode)|DST(AFP|AIND)) || body->m ||
                       body->d<64 || body->d%width || (unsigned long)body->d>child->bytes || width>child->bytes-body->d)return 0;
                    bit=body->d/8;if(!!(child->map[bit/8]&(128>>(bit%8)))!=root)return 0;
                    if(mode==AIMM){if(root || width==8 || body->s< -536870912L || body->s>536870911L || (width==1 && (body->s<0 || body->s>255)))return 0;}
                    else{extent=mode==AFP?f->bytes:s->databytes;
                        if((mode!=AFP && mode!=AMP) || !native64typedcodeoperand(mode,body->s,f->bytes,s->databytes) || body->s%width || (unsigned long)body->s>extent || width>extent-body->s)return 0;
                        map=mode==AFP?f->map:s->datamap;bit=body->s/8;if(!!(map[bit/8]&(128>>(bit%8)))!=root)return 0;
                    }
                }

                for(a=0;a<child->count;a++){body=child->code+a;if((body->returned && body->returned<=32) || body->returned==40 || body->returned==56 || (body->returned>=168 && body->returned<=170) || body->returned==171 || body->returned==174 || body->returned==185)return 0;}
            }
            if(found!=1)return 0;
        }
        for(j=0;j<f->nsites;j++){
            site=f->sites+j;if(site->external>1)return 0;
            if(native64geometryresultwidth(site->contract)){
                if(!site->external || site->inherited || site->table>=s->ntables || site->target>=s->tables[site->table].count || site->end<2 || site->end>=f->count)return 0;
                request=s->tables[site->table].imports+site->target;
                if(request->resultcontract!=site->contract)return 0;
                step=f->code+site->end-1;
                if(step->op!=ILEA || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->m || step->d!=32 || step->returned!=(128|site->contract) ||
                   !native64geometryresultmap(f->bytes,f->map,f->mapbytes,step->s,site->contract))return 0;
            }
            if(site->external && site->table<s->ntables && site->target<s->tables[site->table].count){
                request=s->tables[site->table].imports+site->target;
                if((native64geometryresultwidth(request->resultcontract) || native64geometryresultwidth(site->contract)) && request->resultcontract!=site->contract)return 0;
            }
            if(site->contract==72){
                if(site->external){if(site->table>=s->ntables || site->target>=s->tables[site->table].count || s->tables[site->table].imports[site->target].resultcontract!=72)return 0;}
                else if(site->target>=s->nfunctions || s->functions[site->target].resultcontract!=72)return 0;
                if(site->inherited){
                    if(f->resultcontract!=72 || site->end<2 || site->end>=f->count)return 0;step=f->code+site->end-1;
                    if(step->op!=IMOVL || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->s!=32 || step->m || step->d!=32 || step->returned!=64)return 0;
                }else{
                    if(site->end<2 || site->end>=f->count)return 0;step=f->code+site->end-1;
                    if(step->op!=ILEA || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->m || step->d!=32 || step->returned!=(128|72) || step->s<64 || step->s%8 || (unsigned long)step->s>f->bytes || 24>f->bytes-step->s)return 0;
                    for(at=step->s;at<(unsigned long)step->s+24;at+=8){bit=at/8;if((f->map[bit/8]&(128>>(bit%8)))==0)return 0;}
                }
            }
            if(site->external && site->table<s->ntables && site->target<s->tables[site->table].count && (site->contract==72)!=(s->tables[site->table].imports[site->target].resultcontract==72))return 0;
            if(!site->external && site->target<s->nfunctions && (site->contract==72)!=(s->functions[site->target].resultcontract==72))return 0;
            if(site->contract==88){
                if(!site->external || site->inherited || site->table>=s->ntables || site->target>=s->tables[site->table].count || site->end<2 || site->end>=f->count)return 0;
                request=s->tables[site->table].imports+site->target;
                if(strcmp(request->name,"fstat") || request->signature!=0xda4499c2U || request->bytes!=72 || request->mapbytes!=2 || request->map[0] || request->map[1]!=0x80)return 0;
                step=f->code+site->end-1;
                if(step->op!=ILEA || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->m || step->d!=32 || step->returned!=(128|88) || step->s<0 || (unsigned long)step->s>f->bytes ||
                   !native64statresultmap(f->bytes,f->map,f->mapbytes,step->s))return 0;
            }
            if(!site->external){if(site->target>=s->nfunctions)return 0;}
            if(!site->external){
                if((site->contract==48)!=(s->functions[site->target].resultcontract==48) ||
                   (site->inherited && ((site->contract==48)!=(f->resultcontract==48))))return 0;
                if(site->contract==48 && !site->inherited){
                    if(site->end<2 || site->end>=f->count)return 0;
                    step=f->code+site->end-1;
                    if(step->op!=ILEA || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->m ||
                       step->d!=32 || step->returned!=(128|48) || step->s<64 ||
                       (unsigned long)step->s>f->bytes || 16>f->bytes-step->s || step->s%8)return 0;
                    for(at=step->s;at<(unsigned long)step->s+16;at+=8){
                        bit=at/8;if(!(f->map[bit/8]&(0x80>>(bit%8))))return 0;
                    }
                }
            }
            else{
                if(site->table>=s->ntables || site->target>=s->tables[site->table].count ||
                   (site->modulemode!=AFP && site->modulemode!=AMP) || site->moduleoffset%8)return 0;
                bytes=site->modulemode==AFP?f->bytes:s->databytes;map=site->modulemode==AFP?f->map:s->datamap;
                if(site->moduleoffset>bytes || 8>bytes-site->moduleoffset || (site->modulemode==AFP && site->moduleoffset<64))return 0;
                bit=site->moduleoffset/8;if(!(map[bit/8]&(0x80>>(bit%8))))return 0;
            }
        }
        total+=f->bytes;code+=f->count;links+=f->exported;if(maxsites<f->nsites)maxsites=f->nsites;
    }
    for(i=0;i<s->ninitial;i++){
        v=s->initial+i;
        if((v->width!=1 && v->width!=4 && v->width!=8) || v->offset<end || v->offset>s->databytes ||
           v->width>s->databytes-v->offset || v->offset%v->width || (v->width==1 && v->bits>255) || (v->width==4 && v->bits>0xffffffffULL))return 0;
        for(at=v->offset;at<v->offset+v->width;at++){bit=at/8;if(s->datamap[bit/8]&(0x80>>(bit%8)))return 0;}
        end=v->offset+v->width;
    }
    if(!native64moduletablescheck(s))return 0;
    *instructions=code;*exports=links;*scratch=maxsites;*imports=nimports;return 1;
}

typedef struct Native64ModuleBuild Native64ModuleBuild;
struct Native64ModuleBuild {
    const Native64ModuleSpec *spec;
    Module *module;
    Native64TypedCall *calls;
    unsigned int *offsets;
    unsigned int code,exports,scratch,imports;
    int jit,rooted;
};

static char*
native64modulecopy(const char *s)
{
    char *p;unsigned int n=strlen(s)+1;
    p=mallocz(n,0);if(p==nil)error(exNomem);memmove(p,s,n);return p;
}

static int
native64modulemake(void *arg)
{
    Native64ModuleBuild *b=arg;const Native64ModuleSpec *s=b->spec;const Native64ModuleFunction *f;
    Module *m;unsigned int i,j,k,pc=0,link=0,ordinal,bytes=0,triples=0,contexts=0,mapped2=0,mapped3=0,filewrite=0,fileread=0;Import *imports;const Native64ModuleImport *request;const Native64ModuleTable *table;const Native64ModuleSite *site;const Native64ModuleInit *v;
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(s->functions[i].code[j].op==INEWAZ)bytes=1;
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(s->functions[i].code[j].returned==171)triples=1;
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(s->functions[i].code[j].returned==172 || s->functions[i].code[j].returned==174)contexts=1;
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(s->functions[i].code[j].returned==183 || s->functions[i].code[j].returned==185){if(s->functions[i].code[j].s==2)mapped2=1;else mapped3=1;}
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(s->functions[i].code[j].returned==194)mapped2=1;else if(s->functions[i].code[j].returned==196)mapped3=1;
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++){if(s->functions[i].code[j].returned==201)filewrite=1;if(s->functions[i].code[j].returned==202)fileread=1;}
    m=b->module=mallocz(sizeof(*m),1);if(m==nil)error(exNomem);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;
    m->name=native64modulecopy(s->name);m->path=native64modulecopy(s->path);
    m->type=mallocz((s->nfunctions+1+b->imports+bytes+triples+mapped2+mapped3+filewrite+fileread+contexts)*sizeof(Type*),1);if(m->type==nil)error(exNomem);m->ntype=s->nfunctions+1+b->imports+bytes+triples+mapped2+mapped3+filewrite+fileread+contexts;
    m->type[0]=dtype(freeheap,s->databytes,(uchar*)s->datamap,s->mapbytes);if(m->type[0]==nil)error(exNomem);
    for(i=0;i<s->nfunctions;i++){f=s->functions+i;m->type[i+1]=dtype(freeheap,f->bytes,(uchar*)f->map,f->mapbytes);if(m->type[i+1]==nil)error(exNomem);}
    ordinal=s->nfunctions+1;
    if(s->ntables){m->ldt=mallocz((s->ntables+1)*sizeof(Import*),1);if(m->ldt==nil)error(exNomem);}
    for(i=0;i<s->ntables;i++){
        table=s->tables+i;m->ldt[i]=imports=mallocz((table->count+1)*sizeof(Import),1);if(imports==nil)error(exNomem);
        for(j=0;j<table->count;j++){
            request=table->imports+j;imports[j].name=native64modulecopy(request->name);memmove(&imports[j].sig,&request->signature,4);
            m->type[ordinal]=dtype(freeheap,request->bytes,(uchar*)request->map,request->mapbytes);if(m->type[ordinal++]==nil)error(exNomem);
        }
    }
    if(bytes){m->type[ordinal]=dtype(freeheap,1,nil,0);if(m->type[ordinal++]==nil)error(exNomem);}
    if(triples){uchar roots[]={0xe0};m->type[ordinal]=dtype(freeheap,24,roots,1);if(m->type[ordinal++]==nil)error(exNomem);}
    if(mapped2){uchar pairmap[]={0xc0};m->type[ordinal]=dtype(freeheap,16,pairmap,1);if(m->type[ordinal++]==nil)error(exNomem);}
    if(mapped3){uchar triplemap[]={0xe0};m->type[ordinal]=dtype(freeheap,24,triplemap,1);if(m->type[ordinal++]==nil)error(exNomem);}
    if(filewrite){uchar map[]={0x50};m->type[ordinal]=dtype(freeheap,32,map,1);if(m->type[ordinal++]==nil)error(exNomem);}
    if(fileread){uchar map[]={0x20};m->type[ordinal]=dtype(freeheap,24,map,1);if(m->type[ordinal++]==nil)error(exNomem);}
    if(contexts){uchar contextmap[]={0xe0};m->type[ordinal]=dtype(freeheap,24,contextmap,1);if(m->type[ordinal]==nil)error(exNomem);}
    m->nprog=b->code;m->prog=mallocz(b->code*sizeof(Inst),1);if(m->prog==nil)error(exNomem);
    b->offsets=mallocz(s->nfunctions*sizeof(*b->offsets),1);if(b->offsets==nil)error(exNomem);
    if(b->scratch){b->calls=mallocz(b->scratch*sizeof(*b->calls),1);if(b->calls==nil)error(exNomem);}
    for(i=0;i<s->nfunctions;i++){b->offsets[i]=pc;pc+=s->functions[i].count;}
    for(i=0;i<s->nfunctions;i++){
        f=s->functions+i;
        for(j=0;j<f->nsites;j++){
            site=f->sites+j;
            if(site->external){
                ordinal=s->nfunctions+1+site->target;for(k=0;k<site->table;k++)ordinal+=s->tables[k].count;
                request=s->tables[site->table].imports+site->target;
                b->calls[j]=(Native64TypedCall){site->first,site->end,m->type[ordinal],nil,site->target,site->contract,site->inherited,1,site->modulemode,site->moduleoffset,request->resultcontract};
            }else b->calls[j]=(Native64TypedCall){site->first,site->end,m->type[site->target+1],m->prog+b->offsets[site->target],site->target+1,site->contract,site->inherited,0,0,0,s->functions[site->target].resultcontract};
        }
        if(!native64typedmulticode(m->prog+b->offsets[i],f->count,f->code,f->count,f->bytes,s->databytes,b->calls,f->nsites))error(exInval);
        for(j=0;j<f->count;j++){
            if(f->code[j].op==INEWAZ)m->prog[b->offsets[i]+j].reg=s->nfunctions+1+b->imports;
            if(f->code[j].returned==194)m->prog[b->offsets[i]+j].reg=s->nfunctions+1+b->imports+bytes+triples;
            if(f->code[j].returned==201 || f->code[j].returned==202)m->prog[b->offsets[i]+j].reg=s->nfunctions+1+b->imports+bytes+triples+mapped2+mapped3+(f->code[j].returned==202?filewrite:0);
            if(f->code[j].returned==196)m->prog[b->offsets[i]+j].reg=s->nfunctions+1+b->imports+bytes+triples+mapped2;
            if(f->code[j].returned==171)m->prog[b->offsets[i]+j].reg=s->nfunctions+1+b->imports+bytes;
            if(f->code[j].returned==183 || f->code[j].returned==185)m->prog[b->offsets[i]+j].s.imm=s->nfunctions+1+b->imports+bytes+triples+(f->code[j].s==3?mapped2:0);
            if(f->code[j].returned==172 || f->code[j].returned==174)m->prog[b->offsets[i]+j].s.imm=m->ntype-1;
        }
    }
    m->ext=mallocz((b->exports+1)*sizeof(Link),1);if(m->ext==nil)error(exNomem);
    for(i=0;i<s->nfunctions;i++)if(s->functions[i].exported){
        f=s->functions+i;m->ext[link].name=native64modulecopy(f->name);
        memmove(&m->ext[link].sig,&f->signature,4);m->ext[link].frame=m->type[i+1];m->ext[link++].u.pc=m->prog+b->offsets[i];
    }
    if(s->databytes){m->origmp=H2D(uchar*,heapz(m->type[0]));ptradd(D2H(m->origmp));b->rooted=1;}
    for(i=0;i<s->ninitial;i++){v=s->initial+i;
        if(v->width==1){BYTE x=v->bits;memmove(m->origmp+v->offset,&x,1);}
        else if(v->width==4){UWORD x=v->bits;memmove(m->origmp+v->offset,&x,4);}
        else memmove(m->origmp+v->offset,&v->bits,8);
    }
    native64moduletablesapply(s,m->origmp,b->offsets);
    for(i=0;i<s->nstrings;i++){
        String *text=s->string((char*)s->strings[i].text,s->strings[i].bytes);
        if(text==nil || text==H)error(exInval);
        memmove(m->origmp+s->strings[i].offset,&text,sizeof(text));
    }
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].ncases;j++)
        if(s->functions[i].cases[j].op==ICASEC)m->native64casec=native64modulecasecstep;
    m->native64importload=s->load;
    if(s->entry!=~0U){m->entry=m->prog+b->offsets[s->entry];m->entryt=m->type[s->entry+1];}
    m->cstep=native64typedlea8step;
    if(b->jit && !compile(m,m->nprog,nil))error(exCompile);
    return 1;
}

/* Publish exactly one owned, unregistered Module. Keep output/REG unchanged on
 * refusal or protected allocation/compiler failure. No namespace/cache changes. */
static int
native64typedmodule(const Native64ModuleSpec *spec,int jit,Native64ModuleOwner *result)
{
    Native64ModuleBuild b={0};REG saved;int ok;
    if(result==nil || result->module!=nil || result->rooted || result->self!=nil || (jit!=0 && jit!=1) || !native64modulecheck(spec,&b.code,&b.exports,&b.scratch,&b.imports))return 0;
    b.spec=spec;b.jit=jit;memmove(&saved,&R,sizeof(saved));ok=kprotect(native64modulemake,&b);
    memmove(&R,&saved,sizeof(R));free(b.calls);free(b.offsets);
    if(!ok){if(b.rooted)ptrdel(D2H(b.module->origmp));if(b.module!=nil)unload(b.module);memmove(&R,&saved,sizeof(R));return 0;}
    result->module=b.module;result->rooted=b.rooted;result->self=result;return 1;
}

/* Keep the guard alive until all module instances/stacks have retired. */
static int
native64typedmoduleclear(Native64ModuleOwner *owner)
{
    Module *m;int rooted;
    if(owner==nil)return 0;
    if(owner->module==nil)return owner->rooted==0 && owner->self==nil;
    m=owner->module;if(owner->self!=owner || m->ref!=1)return 0;
    rooted=owner->rooted;owner->module=nil;owner->rooted=0;owner->self=nil;
    if(rooted)ptrdel(D2H(m->origmp));unload(m);return 1;
}
#endif
