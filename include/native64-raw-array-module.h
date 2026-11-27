#ifndef NATIVE64_RAW_ARRAY_MODULE_H
#define NATIVE64_RAW_ARRAY_MODULE_H

/* Call only after packed operands have passed the corresponding shape check. */
static int
native64rawarraymap(const Native64ModuleSpec *spec, const Native64ModuleFunction *fn,
                     unsigned int mode, unsigned int value, int root)
{
 const uchar *map;
 unsigned int bit;
 if(mode==AIMM)
  return !root;
 if(mode&AIND){
  value&=65535;
  mode&=~AIND;
  root=1;
 }
 map=mode==AFP?fn->map:spec->datamap;
 bit=value/8;
 return !!(map[bit/8]&(128>>(bit%8)))==root;
}

static int
native64modulerawindex(const Native64ModuleSpec *s, const Native64ModuleFunction *f,
                        unsigned int pc)
{
 const Native64TypedStep *v=f->code+pc;
 unsigned int i;
 if(!native64typedrawindex(v,f->bytes,s->databytes)||
    !native64rawarraymap(s,f,USRC(v->mode),v->s,1))
  return 0;
 if(v->returned==Native64TypedBorrowIndex)
  return 1;
 if(!native64rawarraymap(s,f,UDST(v->mode),v->d,0))
  return 0;
 if(!native64rawfieldindex(v))
  return 1;
 if(pc+1>=f->count||!native64rawfieldpair(v,v+1,f->bytes,s->databytes))
  return 0;
 for(i=0;i<f->nsites;i++)
  if(f->sites[i].first<=pc+1&&pc<=f->sites[i].end)
   return 0;
 return 1;
}

static int
native64modulerawallocation(const Native64ModuleSpec *s, const Native64ModuleFunction *f,
                             const Native64TypedStep *v)
{
 unsigned int middle=v->mode&ARM;
 if(!(v->op==INEWAZ?native64typedrawnew(v,f->bytes,s->databytes):
      native64typedrawslice(v,f->bytes,s->databytes)))
  return 0;
 if(!native64rawarraymap(s,f,UDST(v->mode),v->d,1)||
    !native64rawarraymap(s,f,USRC(v->mode),v->s,v->op==ISLICELA))
  return 0;
 return middle==AXIMM||
  native64rawarraymap(s,f,middle==AXINF?AFP:AMP,v->m&65535,0);
}

static int
native64modulerawargindex(const Native64ModuleSpec *s, const Native64ModuleFunction *f, unsigned int pc)
{
 const Native64TypedStep *prep=f->code+pc;
 unsigned int i,mm=prep->mode&ARM;
 if(pc+2>=f->count||!native64rawargindexchain(prep,f->bytes,s->databytes)||
    !native64rawarraymap(s,f,USRC(prep->mode),prep->s,0)||
    !native64rawarraymap(s,f,AFP,prep->d,0)||
    ((mm==AXINF||mm==AXINM)&&!native64rawarraymap(s,f,mm==AXINF?AFP:AMP,prep->m,0)))
  return 0;
 for(i=0;i<f->nsites;i++)
  if(!f->sites[i].external&&f->sites[i].first<pc&&pc+2<f->sites[i].end)
   return 1;
 return 0;
}

static int
native64modulerawrecord(const Native64ModuleSpec *s, const Native64ModuleFunction *f,
                        unsigned int pc)
{
 const Native64TypedStep *step=f->code+pc;
 int side=native64rawrecordside(step);
 unsigned int i;
 if(!native64typedrawrecord(step,f->bytes,s->databytes)||
    !native64rawarraymap(s,f,side?UDST(step->mode):USRC(step->mode),side?step->d:step->s,1))
  return 0;
 for(i=0;i<f->nsites;i++)
  if(f->sites[i].first<=pc&&pc<=f->sites[i].end)
   return 0;
 return 1;
}

/* Track both non-GC scratch addresses through every edge. A live mapped FP or MP
 * array owner must dominate each raw use and remain unchanged until that use.
 * The checked field-index pair uses its address within one VM step only. */
static int
native64modulerawarrays(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{unsigned int owners[512][2],queue[1537],head=0,tail=0,i,j,k,at,has=0,flow[2],out[2],edge,to,slot,cl,off,n,mode,np,bit,bytes;uchar reached[512];const Native64TypedStep*v;const Native64ModuleCase*t;const Native64ModuleSite*site;const uchar*map;
 for(i=0;i<f->count;i++){
  unsigned int role=f->code[i].returned;
  if(role==65||role==79||role==80||role==Native64TypedRecordArray||
     role==Native64TypedBorrowIndex||role==Native64TypedArrayArgIndex)has=1;
 }
 if(!has)return 1;
 if(f->count>512||f->ncases>f->count||(f->ncases&&(f->cases==nil||f->cases==H)))return 0;
 for(i=0;i<f->ncases;i++){t=f->cases+i;if(t->pc>=f->count||t->count>512||(t->count&&(t->rows==nil||t->rows==H)))return 0;}
 for(i=0;i<f->count;i++){v=f->code+i;
  if(v->returned==79){if(!native64modulerawallocation(s,f,v))return 0;continue;}
  if(v->returned==80||v->returned==Native64TypedBorrowIndex){if(!native64modulerawindex(s,f,i))return 0;continue;}
  if(v->returned==Native64TypedArrayArgIndex){if(!native64modulerawargindex(s,f,i))return 0;continue;}
  if(v->returned==Native64TypedRecordArray){if(!native64modulerawrecord(s,f,i))return 0;continue;}
  if(v->returned!=65)continue;if(!native64typedrawuse(v,f->bytes,s->databytes))return 0;n=native64rawusewidth(v);
  for(k=0;k<2;k++){mode=k?UDST(v->mode):USRC(v->mode);off=k?v->d:v->s;
   if(k&&native64rawbranch(v->op))continue;if(mode==(AFP|AIND)){if(native64rawslot(off))continue;if(k&&off==32&&(v->op==IMOVW||v->op==IMOVP)){if(f->resultcontract)return 0;continue;}
    site=nil;for(j=0;j<f->nsites;j++)if(f->sites[j].first<i&&i<f->sites[j].end){if(site!=nil)return 0;site=f->sites+j;}if(site==nil)return 0;
    if(site->external){if(site->table>=s->ntables||site->target>=s->tables[site->table].count)return 0;bytes=s->tables[site->table].imports[site->target].bytes;map=s->tables[site->table].imports[site->target].map;np=s->tables[site->table].imports[site->target].mapbytes;}
    else{if(site->target>=s->nfunctions)return 0;bytes=s->functions[site->target].bytes;map=s->functions[site->target].map;np=s->functions[site->target].mapbytes;}
    if(off>bytes||n>bytes-off||map==nil||map==H||np!=(bytes+63)/64)return 0;
   }else if(mode==AIMM)continue;else map=mode==AFP?f->map:s->datamap;
   for(j=0;j<((!k&&v->op==IHEADM)?8:n);j++){bit=(off+j)/8;if(!!(map[bit/8]&(128>>(bit%8)))!=(v->op==IMOVP||(v->op==IMOVMP&&j<8*native64arrayroots(v->m&65535))||(!k&&v->op==IHEADM)))return 0;}
  }
  if(v->op!=IMOVM&&v->op!=IHEADM&&v->op!=IMOVMP&&v->op!=IMOVP&&((v->mode&ARM)==AXINF||(v->mode&ARM)==AXINM)){map=(v->mode&ARM)==AXINF?f->map:s->datamap;bit=(v->m&65535)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
 }
 memset(reached,0,sizeof(reached));memset(owners,0,sizeof(owners));reached[0]=1;queue[tail++]=0;
 while(head<tail){i=queue[head++];v=f->code+i;flow[0]=owners[i][0];flow[1]=owners[i][1];
  if(v->returned==Native64TypedBorrowIndex){
   at=flow[(v->d-48)/8];
   if(!at||!native64arrayword(at&63,(unsigned long)v->m>>16))return 0;
  }
  if(v->returned==80||v->returned==Native64TypedBorrowIndex){
   slot=((v->m&255)-48)/8;
   flow[slot]=native64rawfieldindex(v)?0:(v->s<<6)|((v->m>>8)&255)|(USRC(v->mode)==AMP?0x80000000U:0);
  }
  else if(v->returned==65||v->returned==Native64TypedRecordArray){
   if(!i||!native64rawfieldindex(v-1))
    for(k=0;k<2;k++){
     mode=k?UDST(v->mode):USRC(v->mode);at=k?v->d:v->s;
     if(mode!=(AFP|AIND)||!native64rawslot(at))continue;
     slot=(at-48)/8;cl=flow[slot]&63;off=k?(v->m>>16)&255:(v->m>>24)&255;
     if(!flow[slot]||!native64rawusefits(v,cl,off))return 0;
    }
  }
  mode=UDST(v->mode);
  if(v->op==IINDW||v->op==IINDL||v->op==IINDB||v->op==IINDF||v->op==IINDX){
   if(v->returned!=80&&v->returned!=Native64TypedBorrowIndex&&
      (v->mode&ARM)==AXINF&&native64rawslot(v->m))flow[(v->m-48)/8]=0;
  }else if((mode==AFP||mode==AMP)&&v->op!=ISLICELA){
   for(k=0;k<2;k++){
    if(mode==AFP&&v->d==48+8*k)flow[k]=0;
    if(flow[k]&&((flow[k]>>31)==(mode==AMP))&&v->d>=(mode==AFP?64:0)&&
       (unsigned long)v->d/8==((flow[k]&0x7fffffffU)>>6)/8)flow[k]=0;
   }
  }
  if(v->op==ICALL||v->op==IMCALL||v->op==ISPAWN)for(k=0;k<2;k++)if(flow[k]&0x80000000U)flow[k]=0;
  if(v->op==IRET||v->op==IEXIT||v->op==IRAISE)continue;t=nil;for(j=0;j<f->ncases;j++)if(f->cases[j].pc==i)t=f->cases+j;
  for(edge=0;edge<(t?t->count+(t->op!=IGOTO):native64typedbranch(v->op)?(v->op==IJMP?1:2):1);edge++){
   to=t?(edge<t->count?t->rows[edge].target:t->fallback):native64typedbranch(v->op)&&!edge?v->d:i+1;if(to>=f->count)return 0;
   for(k=0;k<2;k++)out[k]=!reached[to]?flow[k]:owners[to][k]==flow[k]?flow[k]:0;
   if(!reached[to]||out[0]!=owners[to][0]||out[1]!=owners[to][1]){if(tail>=nelem(queue))return 0;reached[to]=1;owners[to][0]=out[0];owners[to][1]=out[1];queue[tail++]=to;}
  }
 }return 1;}
#endif
