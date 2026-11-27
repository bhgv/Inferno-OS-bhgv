#include "fd.h"
static unsigned fdhigh;
static void fdcall(unsigned body,int jit,int grow,int q,int nonnil){
 const FDBody*b=fd_bodies+body;uchar map[]={0,0x80};Module*m;Modlink*link;Prog process;Frame*entry,*source;uchar*root;Inst*p;void*owner=H,*result;WORD value;uvlong guard=0xfedc123456789abcULL,after;int i,initial;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;
 m->name=strdup("real-Sys-FD-nil");m->path=strdup("real-Sys-FD-nil");require(m->name&&m->path);
 m->ntype=4;m->type=mallocz(4*sizeof(Type*),1);require(m->type!=nil);
 m->type[0]=dtype(freeheap,fd_databytes,(uchar*)fd_datamap,sizeof(fd_datamap));
 m->type[1]=dtype(freeheap,96,map,2);m->type[2]=dtype(freeheap,b->bytes,(uchar*)b->map,(b->bytes+63)/64);m->type[3]=dtype(freeheap,8,nil,0);
 for(i=0;i<4;i++)require(m->type[i]!=nil);m->entryt=m->type[1];m->ss=grow?0:4096;
 m->nprog=5+b->count;m->prog=mallocz(m->nprog*sizeof(Inst),1);require(m->prog!=nil);p=m->prog;
 callinstruction(p,IFRAME,AIMM,AFP,2,40);callargument(p+1,IMOVP,AFP,64,40,b->arg);callargument(p+2,IMOVL,AFP,48,40,32);
 callinstruction(p+3,ICALL,AFP,AIMM,40,0);p[3].d.ins=p+5;callinstruction(p+4,IEXIT,AXXX,AXXX,0,0);
 require(native64typedcode(p+5,b->count,b->code,b->count,b->bytes,fd_databytes));m->entry=p;m->origmp=H2D(uchar*,heapz(m->type[0]));
 if(jit)require(compile(m,m->nprog,nil));memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
 process.R.M=link;process.R.PC=m->entry;entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(m->entryt,entry);source=entry;R.s=&source;newstack(&process);free(entry);root=process.R.FP;
 if(nonnil){owner=H2D(void*,heapz(m->type[3]));if((uintptr)owner>0xffffffffULL)fdhigh++;}memmove(root+64,&owner,8);result=root+72;memmove(root+48,&result,8);memset(root+72,0x5a,8);memmove(root+80,&guard,8);memset(root+88,0xa3,8);
 process.quanta=q;initial=raised;message[0]='x';for(i=0;i<1000;i++)if(!kprotect(execute,&process))break;
 require(i<1000&&raised==initial+1&&!worker.nerr&&!message[0]);memmove(&value,root+72,4);require(value==(nonnil?29:17));
 for(i=76;i<80;i++)require(root[i]==0x5a);memmove(&after,root+80,8);require(after==guard);for(i=88;i<96;i++)require(root[i]==0xa3);
 if(nonnil)require(D2H(owner)->ref==1);require(R.FP==root);destroystack(&R);require(m->ref==1);unload(m);require(live==0);
}
int main(void){unsigned b;int jit,grow,q,n;require(sizeof(void*)==8&&sizeof(WORD)==4);optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMOVP]=movp;optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IBEQL]=beql;optab[IBNEL]=bnel;optab[IEXIT]=iexit;for(b=0;b<nelem(fd_bodies);b++)for(jit=0;jit<2;jit++)for(grow=0;grow<2;grow++)for(q=1;q<=9;q+=4)for(n=0;n<2;n++)fdcall(b,jit,grow,q,n);printf("PASS Sys.FD/nil: 96 C/JIT calls, %u high-address roots, stack growth/quanta, unchanged roots and scalar result guards\n",fdhigh);return 0;}
