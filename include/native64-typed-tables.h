#ifndef NATIVE64_TYPED_TABLES_H
#define NATIVE64_TYPED_TABLES_H
/* Private trusted compiler metadata. Targets remain Dis instruction ordinals;
 * neither serialized native pointers nor legacy native-PC compilation is used. */
static unsigned int
native64moduletablebytes(const Native64ModuleCase *t)
{
    return t->op==IGOTO?4+4*t->count:t->op==ICASE?8+12*t->count:12+24*t->count;
}

static int
native64moduletableequal(const Native64ModuleCase *a,const Native64ModuleCase *b)
{
    unsigned int i;
    if(a->offset!=b->offset || a->op!=b->op || a->count!=b->count || a->fallback!=b->fallback)return 0;
    for(i=0;i<a->count;i++)if(a->rows[i].low!=b->rows[i].low || a->rows[i].high!=b->rows[i].high || a->rows[i].target!=b->rows[i].target)return 0;
    return 1;
}

/* Check every table and target before allocation. A matching table declaration
 * is mandatory for each table opcode; code and ordinary initializers cannot
 * supply conflicting owners for the same MP bytes. */
static int
native64moduletablescheck(const Native64ModuleSpec *s)
{
    unsigned int i,j,k,l,n=0,rows=0,bytes,other,at,width,matches;
    const Native64ModuleFunction *f,*g;const Native64ModuleCase *t,*u;
    const Native64ModuleCaseRow *r;const Native64TypedStep *step;
    for(i=0;i<s->nfunctions;i++){
        f=s->functions+i;
        if(f->ncases>256-n || (f->ncases && (f->cases==nil || f->cases==H)))return 0;
        n+=f->ncases;
        for(j=0;j<f->ncases;j++){
            t=f->cases+j;
            if(t->pc>=f->count || (j && t->pc<=f->cases[j-1].pc) ||
               (t->op!=ICASE && t->op!=ICASEL && t->op!=IGOTO) || t->count>4096 || t->count>65536-rows ||
               (t->count && (t->rows==nil || t->rows==H)) || (t->op==IGOTO && (!t->count || t->fallback!=~0U)) ||
               (t->op!=IGOTO && t->fallback>=f->count))return 0;
            rows+=t->count;bytes=native64moduletablebytes(t);
            if(t->offset%(t->op==ICASEL?8:4) || t->offset>s->databytes || bytes>s->databytes-t->offset)return 0;
            for(at=t->offset;at<t->offset+bytes;at+=4)if(s->datamap[at/64]&(0x80>>((at/8)%8)))return 0;
            step=f->code+t->pc;width=t->op==ICASEL?8:4;
            if(step->op!=t->op || UDST(step->mode)!=AMP || (USRC(step->mode)!=AFP && USRC(step->mode)!=AMP) ||
               (step->mode&ARM)!=AXNON || step->d!=(long)(t->offset+(t->op==IGOTO?4:0)) || step->m || step->returned ||
               !native64typedcodeoperand(USRC(step->mode),step->s,f->bytes,s->databytes) || step->s%width ||
               width>(USRC(step->mode)==AFP?f->bytes:s->databytes)-step->s)return 0;
            for(k=0;k<t->count;k++){
                r=t->rows+k;
                if(r->target>=f->count || r->low>r->high || (k && r->low<=t->rows[k-1].high) ||
                   (t->op!=ICASEL && (r->low<(-2147483647LL-1) || r->high>2147483647LL)) ||
                   (t->op==IGOTO && (r->low!=r->high || (k && r->low!=t->rows[k-1].low+1))))return 0;
            }
            for(k=0;k<f->nsites;k++){
                if(t->op!=IGOTO && t->fallback>f->sites[k].first && t->fallback<=f->sites[k].end)return 0;
                for(l=0;l<t->count;l++)if(t->rows[l].target>f->sites[k].first && t->rows[l].target<=f->sites[k].end)return 0;
            }
            for(k=0;k<s->ninitial;k++)if(s->initial[k].offset<t->offset+bytes && t->offset<s->initial[k].offset+s->initial[k].width)return 0;
            for(k=0;k<=i;k++){
                g=s->functions+k;
                for(l=0;l<(k==i?j:g->ncases);l++){
                    u=g->cases+l;other=native64moduletablebytes(u);
                    if(u->offset<t->offset+bytes && t->offset<u->offset+other &&
                       (k!=i || !native64moduletableequal(t,u)))return 0;
                }
            }
        }
        for(k=0;k<f->count;k++)if(f->code[k].op==ICASE || f->code[k].op==ICASEL || f->code[k].op==IGOTO){
            matches=0;for(j=0;j<f->ncases;j++)matches+=f->cases[j].pc==k;
            if(matches!=1)return 0;
        }
    }
    return 1;
}

static void
native64moduletableword(uchar *p,unsigned int bits)
{
    UWORD value=bits;memmove(p,&value,4);
}

/* This runs only after complete metadata proof, into an unpublished owned MP.
 * Add the owning function's final program offset to each local target. */
static void
native64moduletablesapply(const Native64ModuleSpec *s,uchar *data,const unsigned int *offsets)
{
    unsigned int i,j,k,header,stride;const Native64ModuleCase *t;uchar *p,*row;uvlong low,high;
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].ncases;j++){
        t=s->functions[i].cases+j;p=data+t->offset;
        native64moduletableword(p,t->count);
        header=t->op==ICASEL?8:4;stride=t->op==IGOTO?4:t->op==ICASE?12:24;
        for(k=0;k<t->count;k++){
            row=p+header+k*stride;
            if(t->op==IGOTO)native64moduletableword(row,offsets[i]+t->rows[k].target);
            else{
                low=t->rows[k].low;high=t->rows[k].high;
                /* The compiler guards signed maxima before table search. */
                if(t->op==ICASE){native64moduletableword(row,low);native64moduletableword(row+4,high==0x7fffffffULL?high:high+1);}
                else{if(high!=0x7fffffffffffffffULL)high++;memmove(row,&low,8);memmove(row+8,&high,8);}
                native64moduletableword(row+(t->op==ICASE?8:16),offsets[i]+t->rows[k].target);
            }
        }
        if(t->op!=IGOTO)native64moduletableword(p+header+t->count*stride,offsets[i]+t->fallback);
    }
}
#endif
