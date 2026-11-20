#ifndef NATIVE64_TYPED_ALT_H
#define NATIVE64_TYPED_ALT_H
/* Complete nonallocating borrowed guard islands in trusted compiler IR. */
static int
native64modulealts(const Native64ModuleSpec*s,const Native64ModuleFunction*f)
{
 unsigned int pc,i,j,k,n,first,mask,bit,mode,w,roots,total=0,covered=0,guards[32],payloads[32],end,limit;long base,offset;const uchar*map;const Native64TypedStep*a,*c,*p,*h;
 for(i=0;i<f->count;i++)if((f->code[i].returned>=189 && f->code[i].returned<=193) || f->code[i].returned==134 || f->code[i].returned==195 || f->code[i].returned==197 || (f->code[i].returned==198 || f->code[i].returned==199 || f->code[i].returned==203))total++;
 for(pc=0;pc<f->count;pc++)if(f->code[pc].returned==191){
  a=f->code+pc;if(!native64typedaltstep(a,f->bytes,s->databytes))return 0;n=a->m;base=a->s;
  if(pc<2+2*n)return 0;end=pc-2;
  for(i=0;i<n;i++){
   if(!end)return 0;payloads[n-1-i]=end-1;p=f->code+end-1;if(!native64typedaltstep(p,f->bytes,s->databytes) || p->op!=ILEA)return 0;
   w=native64fileiowidth(p->m)?native64fileiowidth(p->m):p->m==72?24:p->m==48?16:p->m&15;end--;limit=0;
   while(end && !f->code[end-1].returned){Inst probe;const Native64TypedStep*t=f->code+end-1;unsigned int width=(t->op==IMOVP || t->op==ISLICEA)?8:4,length=0;
    if(p->m==48 && end+1<payloads[n-1-i] && t->op==ILENA && f->code[end].op==IMOVP && f->code[end+1].op==ISLICEA && f->code[end].s==t->s && USRC(f->code[end].mode)==USRC(t->mode) && f->code[end].d==p->s && f->code[end+1].d==p->s && (f->code[end+1].mode&ARM)==AXINF && t->d==f->code[end+1].m)length=1;
    if(++limit>8 || (!length && ((t->op!=IMOVP && t->op!=IMOVW && !(t->op==ISLICEA && p->m==48)) || UDST(t->mode)!=USRC(p->mode) || t->d<p->s || t->d+width>p->s+w)))break;
    if(t->d<base+8+16*n && base<t->d+width)return 0;
    if(!native64typedcode(&probe,1,t,1,f->bytes,s->databytes))return 0;
    if(t->op!=ISLICEA){unsigned int sm=USRC(t->mode),dm=UDST(t->mode);
     if(t->mode!=(SRC(sm)|DST(dm)) || t->m || !(t->op==IMOVP?native64typedrootoperand(dm,t->d,f->bytes,s->databytes):native64typedwordoperand(dm,t->d,f->bytes,s->databytes)) ||
        !((t->op==IMOVP || t->op==ILENA)?native64typedrootoperand(sm,t->s,f->bytes,s->databytes):native64typedwordoperand(sm,t->s,f->bytes,s->databytes)))return 0;
    }
    mode=UDST(t->mode);map=mode==AFP?f->map:s->datamap;bit=t->d/8;if(!!(map[bit/8]&(128>>(bit%8)))!=(t->op==IMOVP || t->op==ISLICEA))return 0;
    mode=USRC(t->mode);if(mode!=AIMM){map=mode==AFP?f->map:s->datamap;bit=t->s/8;if(!!(map[bit/8]&(128>>(bit%8)))!=(t->op==IMOVP || t->op==ILENA))return 0;}
    if(t->op==ISLICEA && (t->mode&ARM)!=AXIMM){mode=(t->mode&ARM)==AXINF?AFP:AMP;map=mode==AFP?f->map:s->datamap;bit=t->m/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
    end--;
   }
   if(!end)return 0;guards[n-1-i]=--end;
  }
  first=end;mask=0;
  for(i=0;i<8+16*n;i++){bit=(base+i)/8;if(bit/8<f->mapbytes && (f->map[bit/8]&(128>>(bit%8))))return 0;}
  bit=a->d/8;if(bit/8<f->mapbytes && (f->map[bit/8]&(128>>(bit%8))))return 0;
  if(a->d>=base && a->d<base+8+16*n)return 0;
  for(i=0;i<n;i++){
   c=f->code+guards[i];p=f->code+payloads[i];
   if((c->returned!=189 && c->returned!=192 && c->returned!=193 && c->returned!=198) || (p->returned!=190 && p->returned!=134 && p->returned!=195 && p->returned!=197 && p->returned!=199 && p->returned!=203) || !native64typedaltstep(c,f->bytes,s->databytes) || !native64typedaltstep(p,f->bytes,s->databytes) ||
      c->d<base+8 || c->d>=base+8+16*n || (c->d-base-8)%16 || p->d!=c->d+8)return 0;
   if(c->returned==198 && (p->returned!=(c->m==8?199:203) || p->m!=(c->m==8?104:120)))return 0;
   if((c->returned==192 && p->m!=24) || (c->returned==193 && p->m!=(c->m==0?4:24)))return 0;
   k=(c->d-base-8)/16;if(mask&(1U<<k))return 0;mask|=1U<<k;
   mode=USRC(c->mode)&~AIND;
   if(mode==USRC(p->mode)){w=native64fileiowidth(p->m)?native64fileiowidth(p->m):p->m==72?24:p->m==48?16:p->m&15;if(c->s>=p->s && c->s<p->s+w)return 0;}
   for(j=first;j<pc-2;j++)if(!f->code[j].returned){const Native64TypedStep*t=f->code+j;unsigned int width=t->op==IMOVP || t->op==ISLICEA?8:4;if(UDST(t->mode)==mode && t->d<c->s+8 && c->s<t->d+width)return 0;}
   map=mode==AFP?f->map:s->datamap;bit=c->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
   mode=USRC(p->mode);w=native64fileiowidth(p->m)?native64fileiowidth(p->m):p->m==72?24:p->m==48?16:p->m&15;roots=p->m==24 || p->m==48 || p->m==72;map=mode==AFP?f->map:s->datamap;
   for(j=0;j<w;j++){bit=(p->s+j)/8;if(!!(bit/8<(mode==AFP?f->mapbytes:s->mapbytes) && (map[bit/8]&(128>>(bit%8))))!=(native64fileiowidth(p->m)?native64fileioroot(p->m,j):roots))return 0;}
   if(mode==AFP && a->d<p->s+w && p->s<a->d+4)return 0;
  }
  for(i=0;i<2;i++){h=f->code+pc-2+i;if(h->op!=IMOVW || h->mode!=(SRC(AIMM)|DST(AFP)) || h->m || h->returned || h->d!=base+4*i || h->s<0 || h->s>n)return 0;}
  if(f->code[pc-2].s+f->code[pc-1].s!=n)return 0;
  for(i=0;i<f->nsites;i++)if(f->sites[i].first<=pc && f->sites[i].end>=first)return 0;
  for(i=0;i<f->count;i++)if(native64typedbranch(f->code[i].op) && f->code[i].d>first && f->code[i].d<=pc)return 0;
  if(pc+1>=f->count || f->code[pc+1].op!=IGOTO || f->code[pc+1].s!=a->d || USRC(f->code[pc+1].mode)!=AFP)return 0;
  k=0;for(i=0;i<f->ncases;i++)if(f->cases[i].pc==pc+1){if(f->cases[i].op!=IGOTO || f->cases[i].count!=n+(a->op==INBALT) || f->cases[i].fallback!=~0U)return 0;k++;}if(k!=1)return 0;
  for(i=0;i<f->ncases;i++)for(j=0;j<f->cases[i].count;j++)if(f->cases[i].rows[j].target>first && f->cases[i].rows[j].target<=pc)return 0;
  for(i=0;i<f->ncases;i++)if(f->cases[i].op!=IGOTO && f->cases[i].fallback>first && f->cases[i].fallback<=pc)return 0;
  covered+=2*n+1;
 }
 return total==covered;
}
#endif
