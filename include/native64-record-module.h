#ifndef NATIVE64_RECORD_MODULE_H
#define NATIVE64_RECORD_MODULE_H
static int
native64modulerecordstorage(const Native64ModuleSpec *s, const Native64ModuleFunction *f,
                           unsigned int pc)
{
 const Native64TypedStep *v=f->code+pc;
 unsigned int i,k,mode,offset,bytes,roots,bit;
 long operand;
 const uchar *map;
 if(v->returned==Native64TypedRecordNew){
  if(!native64typedrecordnew(v,f->bytes,s->databytes))return 0;
  mode=UDST(v->mode);
  if(mode&AIND){if(f->resultcontract)return 0;}
  else if(!native64rawarraymap(s,f,mode,v->d,1))return 0;
 }else{
  if(!native64typedrecordcopy(v,f->bytes,s->databytes))return 0;
  bytes=native64recordbytes(v->m);roots=native64recordmask(v->m);
  for(k=0;k<2;k++){
   mode=k?UDST(v->mode):USRC(v->mode);operand=k?v->d:v->s;
   if(mode&AIND){
    if(!native64rawarraymap(s,f,mode,operand,1))return 0;
   }else{
    map=mode==AFP?f->map:s->datamap;offset=operand;
    for(i=0;i<bytes/8;i++){
     bit=offset/8+i;
     if(!!(map[bit/8]&(128>>(bit%8)))!=!!(roots&(1U<<i)))return 0;
    }
    if(!roots)for(i=0;i<bytes;i+=4)if(!native64rawarraymap(s,f,mode,offset+i,0))return 0;
   }
  }
 }
 for(i=0;i<f->nsites;i++)if(f->sites[i].first<=pc&&pc<=f->sites[i].end){
  if(pc==f->sites[i].first||pc==f->sites[i].end)return 0;
  if(v->returned==Native64TypedRecordCopy){
   if(!pc||f->code[pc-1].returned!=Native64TypedRecordClone||f->code[pc-1].op!=IHEADP)return 0;
  }else if(mode!=AFP)return 0;
 }
 return 1;
}

static int
native64modulerecordresult(const Native64ModuleSpec *s, const Native64ModuleFunction *f,
                          unsigned int pc)
{
 const Native64TypedStep *v=f->code+pc;
 unsigned int i,found=0;
 if(!native64typedrecordresult(v,f->bytes)||!native64rawarraymap(s,f,AFP|AIND,v->s,1))
  return 0;
 for(i=0;i<f->nsites;i++){
  const Native64ModuleSite *site=f->sites+i;
  if(site->first<pc&&site->end==pc+1&&site->contract==24&&!site->inherited)found++;
 }
 return found==1;
}

static int
native64modulerecordfields(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int pc,k,j,w[2],r[2],mode,off,n,bit,root,init,sites;const uchar*map;const Native64TypedStep*v;
 Native64TypedStep field;
 for(pc=0;pc<f->count;pc++){
  v=f->code+pc;
  if(v->returned==Native64TypedRecordMask){
   Inst probe;
   if(!pc||v[-1].returned!=Native64TypedRecord||v[-1].op!=IMOVW||
      v[-1].mode!=(SRC(USRC(v[-1].mode))|DST(AFP))||!(USRC(v[-1].mode)&AIND)||
      v[-1].d!=v->d||!native64typedcode(&probe,1,v,1,f->bytes,s->databytes))return 0;
   for(j=0;j<f->nsites;j++)if(f->sites[j].first<=pc&&pc<=f->sites[j].end)return 0;
   continue;
  }
  if(v->returned==Native64TypedRecordNew||v->returned==Native64TypedRecordCopy){
   if(!native64modulerecordstorage(s,f,pc))return 0;
   continue;
  }
  if(v->returned==Native64TypedRecordResult){
   if(!native64modulerecordresult(s,f,pc))return 0;
   continue;
  }
  if(v->returned==Native64TypedRecordPair){
   if(f->resultcontract!=48||!native64typedrecordpair(v,f->bytes,s->databytes)||
      !native64rawarraymap(s,f,USRC(v->mode),v->s,1))return 0;
   for(j=0;j<f->nsites;j++)if(f->sites[j].first<=pc&&pc<=f->sites[j].end)return 0;
   continue;
  }
  init=v->returned==Native64TypedRecordInit;
  if(init){
   if(!pc||(f->code[pc-1].returned!=Native64TypedRecordNew&&
            f->code[pc-1].returned!=Native64TypedRecordInit))return 0;
   field=*v;field.returned=Native64TypedRecord;v=&field;
  }else if(v->returned!=Native64TypedRecord)continue;
  if(!native64typedrecord(v,f->bytes,s->databytes)||!native64recordshape(v->op,w,r)||(native64typedbranch(v->op)&&(unsigned long)v->d>=f->count))return 0;
  for(k=0;k<3;k++){
   if(k==1&&native64typedbranch(v->op))continue;if(k==2&&(v->mode&ARM)==AXNON)continue;
   mode=k==0?USRC(v->mode):k==1?UDST(v->mode):(v->mode&ARM)==AXIMM?AIMM:(v->mode&ARM)==AXINF?AFP:AMP;
   off=k==0?v->s:k==1?v->d:v->m;if(mode==AIMM)continue;
   if(k==1&&mode==(AFP|AIND)&&off==32){if(f->resultcontract)return 0;continue;}
   n=w[k==1];root=r[k==1];if(k==2&&v->op==IINDC){n=4;root=0;}
   if(mode&AIND){off&=65535;mode&=~AIND;n=8;root=1;}
   map=mode==AFP?f->map:s->datamap;
   for(j=0;j<n;j++){bit=(off+j)/8;if(!!(map[bit/8]&(128>>(bit%8)))!=root)return 0;}
  }
  /* Initializers have an owning call and a matching freshly allocated layout. */
  sites=0;
  for(j=0;j<f->nsites;j++)if(f->sites[j].first<=pc&&pc<=f->sites[j].end){
   if(!init||pc==f->sites[j].first||pc==f->sites[j].end)return 0;
   sites++;
  }
  if(init&&sites!=1)return 0;
 }return 1;
}
#endif
