#ifndef NATIVE64_RECORD_MODULE_H
#define NATIVE64_RECORD_MODULE_H
static int
native64modulerecordfields(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int pc,k,j,w[2],r[2],mode,off,n,bit,root;const uchar*map;const Native64TypedStep*v;
 for(pc=0;pc<f->count;pc++){
  v=f->code+pc;if(v->returned!=Native64TypedRecord)continue;
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
  /* Arguments/results borrowed from a pending call use separate protocols. */
  for(j=0;j<f->nsites;j++)if(f->sites[j].first<=pc&&pc<=f->sites[j].end)return 0;
 }return 1;
}
#endif
