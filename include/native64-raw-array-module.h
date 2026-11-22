#ifndef NATIVE64_RAW_ARRAY_MODULE_H
#define NATIVE64_RAW_ARRAY_MODULE_H
/* Track both non-GC scratch addresses through every edge. A live mapped FP or MP
 * array owner must dominate each raw use and remain unchanged until that use. */
static int
native64modulerawarrays(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{unsigned int owners[512][2],queue[1537],head=0,tail=0,i,j,k,at,has=0,flow[2],out[2],edge,to,slot,cl,off,n,mode,np,bit,bytes;uchar reached[512];const Native64TypedStep*v;const Native64ModuleCase*t;const Native64ModuleSite*site;const uchar*map;
 for(i=0;i<f->count;i++)if(f->code[i].returned==65||f->code[i].returned==79||f->code[i].returned==80)has=1;if(!has)return 1;if(f->count>512||f->ncases>f->count||(f->ncases&&(f->cases==nil||f->cases==H)))return 0;
 for(i=0;i<f->ncases;i++){t=f->cases+i;if(t->pc>=f->count||t->count>512||(t->count&&(t->rows==nil||t->rows==H)))return 0;}
 for(i=0;i<f->count;i++){v=f->code+i;
  if(v->returned==79||v->returned==80){if(v->returned==79){if(!(v->op==INEWAZ?native64typedrawnew(v,f->bytes,s->databytes):native64typedrawslice(v,f->bytes,s->databytes)))return 0;off=v->d;mode=USRC(v->mode);at=v->s;if(v->op==ISLICELA){bit=v->s/8;map=USRC(v->mode)==AFP?f->map:s->datamap;if(!(map[bit/8]&(128>>(bit%8))))return 0;mode=AIMM;}if(v->op!=INEWAZ&&(v->mode&ARM)!=AXIMM){map=(v->mode&ARM)==AXINF?f->map:s->datamap;bit=(v->m&65535)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}}else{if(!native64typedrawindex(v,f->bytes,s->databytes))return 0;off=v->s;mode=UDST(v->mode);at=v->d;}
   bit=off/8;map=(v->returned==80?USRC(v->mode):UDST(v->mode))==AFP?f->map:s->datamap;if(!(map[bit/8]&(128>>(bit%8))))return 0;if(mode!=AIMM){map=mode==AFP?f->map:s->datamap;bit=at/8;if(map[bit/8]&(128>>(bit%8)))return 0;}continue;
  }
  if(v->returned!=65)continue;if(!native64typedrawuse(v,f->bytes,s->databytes))return 0;n=native64rawusewidth(v);
  for(k=0;k<2;k++){mode=k?UDST(v->mode):USRC(v->mode);off=k?v->d:v->s;
   if(k&&native64rawbranch(v->op))continue;if(mode==(AFP|AIND)){if(native64rawslot(off))continue;if(k&&off==32&&v->op==IMOVW){if(f->resultcontract)return 0;continue;}
    site=nil;for(j=0;j<f->nsites;j++)if(f->sites[j].first<i&&i<f->sites[j].end){if(site!=nil)return 0;site=f->sites+j;}if(site==nil)return 0;
    if(site->external){if(site->table>=s->ntables||site->target>=s->tables[site->table].count)return 0;bytes=s->tables[site->table].imports[site->target].bytes;map=s->tables[site->table].imports[site->target].map;np=s->tables[site->table].imports[site->target].mapbytes;}
    else{if(site->target>=s->nfunctions)return 0;bytes=s->functions[site->target].bytes;map=s->functions[site->target].map;np=s->functions[site->target].mapbytes;}
    if(off>bytes||n>bytes-off||map==nil||map==H||np!=(bytes+63)/64)return 0;
   }else if(mode==AIMM)continue;else map=mode==AFP?f->map:s->datamap;
   for(j=0;j<((!k&&v->op==IHEADM)?8:n);j++){bit=(off+j)/8;if(!!(map[bit/8]&(128>>(bit%8)))!=(!k&&v->op==IHEADM))return 0;}
  }
  if(v->op!=IMOVM&&v->op!=IHEADM&&((v->mode&ARM)==AXINF||(v->mode&ARM)==AXINM)){map=(v->mode&ARM)==AXINF?f->map:s->datamap;bit=(v->m&65535)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
 }
 memset(reached,0,sizeof(reached));memset(owners,0,sizeof(owners));reached[0]=1;queue[tail++]=0;
 while(head<tail){i=queue[head++];v=f->code+i;flow[0]=owners[i][0];flow[1]=owners[i][1];
  if(v->returned==80){slot=((v->m&255)-48)/8;flow[slot]=(v->s<<2)|((unsigned long)v->m>>8)|(USRC(v->mode)==AMP?0x80000000U:0);}
  else if(v->returned==65){n=native64rawusewidth(v);for(k=0;k<2;k++){mode=k?UDST(v->mode):USRC(v->mode);at=k?v->d:v->s;if(mode!=(AFP|AIND)||!native64rawslot(at))continue;slot=(at-48)/8;cl=flow[slot]&3;off=k?(v->m>>16)&255:(v->m>>24)&255;if(!flow[slot]||!native64rawarraywidth(cl)||off>native64rawarraywidth(cl)||n>native64rawarraywidth(cl)-off)return 0;}}
  else{if((UDST(v->mode)==AFP||UDST(v->mode)==AMP)&&v->op!=IINDW&&v->op!=IINDL&&v->op!=IINDB&&v->op!=IINDF&&v->op!=IINDX&&v->op!=ISLICELA){for(k=0;k<2;k++)if(UDST(v->mode)==AFP&&native64rawslot(v->d)&&v->d==48+8*k)flow[k]=0;for(k=0;k<2;k++)if(flow[k]&&((flow[k]>>31)==(UDST(v->mode)==AMP))&&v->d>=(UDST(v->mode)==AFP?64:0)&&(unsigned long)v->d/8==((flow[k]&0x7fffffffU)>>2)/8)flow[k]=0;}
   if((v->op==IINDW||v->op==IINDL||v->op==IINDB||v->op==IINDF||v->op==IINDX)&&(v->mode&ARM)==AXINF&&native64rawslot(v->m))flow[(v->m-48)/8]=0;
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
