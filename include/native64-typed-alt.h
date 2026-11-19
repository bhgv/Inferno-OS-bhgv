#ifndef NATIVE64_TYPED_ALT_H
#define NATIVE64_TYPED_ALT_H
/* Complete nonallocating borrowed guard islands in trusted compiler IR. */
static int
native64modulealts(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int pc,i,j,k,n,first,mask,bit,mode,w,roots,total=0,covered=0;long base,offset;const uchar*map;const Native64TypedStep*a,*c,*p,*h;
 for(i=0;i<f->count;i++)if(f->code[i].returned>=189 && f->code[i].returned<=193)total++;
 for(pc=0;pc<f->count;pc++)if(f->code[pc].returned==191){
  a=f->code+pc;if(!native64typedaltstep(a,f->bytes,s->databytes))return 0;n=a->m;base=a->s;
  if(pc<2+2*n)return 0;first=pc-2-2*n;mask=0;
  for(i=0;i<8+16*n;i++){bit=(base+i)/8;if(bit/8<f->mapbytes && (f->map[bit/8]&(128>>(bit%8))))return 0;}
  bit=a->d/8;if(bit/8<f->mapbytes && (f->map[bit/8]&(128>>(bit%8))))return 0;
  if(a->d>=base && a->d<base+8+16*n)return 0;
  for(i=0;i<n;i++){
   c=f->code+first+2*i;p=c+1;
   if((c->returned!=189 && c->returned!=192 && c->returned!=193) || p->returned!=190 || !native64typedaltstep(c,f->bytes,s->databytes) || !native64typedaltstep(p,f->bytes,s->databytes) ||
      c->d<base+8 || c->d>=base+8+16*n || (c->d-base-8)%16 || p->d!=c->d+8)return 0;
   if((c->returned==192 && p->m!=24) || (c->returned==193 && p->m!=(c->m==0?4:24)))return 0;
   k=(c->d-base-8)/16;if(mask&(1U<<k))return 0;mask|=1U<<k;
   mode=USRC(c->mode)&~AIND;map=mode==AFP?f->map:s->datamap;bit=c->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
   mode=USRC(p->mode);w=p->m&15;roots=p->m==24;map=mode==AFP?f->map:s->datamap;
   for(j=0;j<w;j++){bit=(p->s+j)/8;if(!!(bit/8<(mode==AFP?f->mapbytes:s->mapbytes) && (map[bit/8]&(128>>(bit%8))))!=roots)return 0;}
   if(mode==AFP && a->d<p->s+w && p->s<a->d+4)return 0;
  }
  for(i=0;i<2;i++){h=f->code+pc-2+i;if(h->op!=IMOVW || h->mode!=(SRC(AIMM)|DST(AFP)) || h->m || h->returned || h->d!=base+4*i || h->s<0 || h->s>n)return 0;}
  if(f->code[pc-2].s+f->code[pc-1].s!=n)return 0;
  for(i=0;i<f->nsites;i++)if(f->sites[i].first<=pc && f->sites[i].end>=first)return 0;
  for(i=0;i<f->count;i++)if(native64typedbranch(f->code[i].op) && f->code[i].d>first && f->code[i].d<=pc)return 0;
  if(pc+1>=f->count || f->code[pc+1].op!=IGOTO || f->code[pc+1].s!=a->d || USRC(f->code[pc+1].mode)!=AFP)return 0;
  k=0;for(i=0;i<f->ncases;i++)if(f->cases[i].pc==pc+1){if(f->cases[i].op!=IGOTO || f->cases[i].count!=n || f->cases[i].fallback!=~0U)return 0;k++;}if(k!=1)return 0;
  for(i=0;i<f->ncases;i++)for(j=0;j<f->cases[i].count;j++)if(f->cases[i].rows[j].target>first && f->cases[i].rows[j].target<=pc)return 0;
  covered+=2*n+1;
 }
 return total==covered;
}
#endif
