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
struct Native64ModuleOwner {Module *module;int rooted;Native64ModuleOwner *self;};
struct Native64ModuleSite {
    unsigned int first,end,target,contract,inherited;
    unsigned int external,table,modulemode,moduleoffset;
};
struct Native64ModuleImport {
    const char *name;unsigned int signature,bytes;
    const uchar *map;unsigned int mapbytes;
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
};
struct Native64ModuleInit {unsigned int offset,width;uvlong bits;};
struct Native64ModuleSpec {
    const char *name,*path;
    const Native64ModuleFunction *functions;
    unsigned int nfunctions,entry,databytes;
    const uchar *datamap;
    unsigned int mapbytes;
    const Native64ModuleInit *initial;
    unsigned int ninitial;
    const Native64ModuleTable *tables;unsigned int ntables;
};

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

/* Immutable readable checked-compiler metadata, not untrusted file inputs. */
static int
native64modulecheck(const Native64ModuleSpec *s,unsigned int *instructions,unsigned int *exports,unsigned int *scratch,unsigned int *imports)
{
    unsigned int i,j,code=0,links=0,maxsites=0,total,end=0,at,bit,nimports=0,bytes;const Native64ModuleFunction *f;const Native64ModuleInit *v;
    const Native64ModuleImport *request;const Native64ModuleTable *table;const Native64ModuleSite *site;const uchar *map;
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
            if(!native64modulename(request->name) || !native64modulemap(request->bytes,request->map,request->mapbytes,1) ||
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
        for(j=0;j<f->nsites;j++){
            site=f->sites+j;if(site->external>1)return 0;
            if(!site->external){if(site->target>=s->nfunctions)return 0;}
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
    Module *m;unsigned int i,j,k,pc=0,link=0,ordinal;Import *imports;const Native64ModuleImport *request;const Native64ModuleTable *table;const Native64ModuleSite *site;const Native64ModuleInit *v;
    m=b->module=mallocz(sizeof(*m),1);if(m==nil)error(exNomem);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;
    m->name=native64modulecopy(s->name);m->path=native64modulecopy(s->path);
    m->type=mallocz((s->nfunctions+1+b->imports)*sizeof(Type*),1);if(m->type==nil)error(exNomem);m->ntype=s->nfunctions+1+b->imports;
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
                b->calls[j]=(Native64TypedCall){site->first,site->end,m->type[ordinal],nil,site->target,site->contract,site->inherited,1,site->modulemode,site->moduleoffset};
            }else b->calls[j]=(Native64TypedCall){site->first,site->end,m->type[site->target+1],m->prog+b->offsets[site->target],site->target+1,site->contract,site->inherited};
        }
        if(!native64typedmulticode(m->prog+b->offsets[i],f->count,f->code,f->count,f->bytes,s->databytes,b->calls,f->nsites))error(exInval);
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
