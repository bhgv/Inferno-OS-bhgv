#include <stdio.h>
static void paircheck(int ok,char *why){if(!ok){fprintf(stderr,"pair IR predicate: %s\n",why);exit(10);}}
static void pairstore(Inst *p,int source,int offset){p->op=IMOVP;p->sm=Afp;p->s.reg=source;p->dm=Afpind;p->d.reg=16;p->d.offset=offset;p->mm=Anone;}
static void pairscalar(Inst *p){p->op=IMOVW;p->sm=Aimm;p->s.offset=0;p->dm=Afp;p->d.reg=40;p->mm=Anone;}
static void paircode(Inst *p,int n){memset(p,0,6*sizeof(*p));for(int i=0;i<n;i++){p[i].pc=i;p[i].op=IRET;p[i].sm=p[i].mm=p[i].dm=Anone;p[i].next=i+1<n?p+i+1:nil;}pairstore(p,32,0);pairstore(p+1,36,4);}
int main(void){
 Type mod={0},list={0},text={0},word={0},tuple={0},fn={0};Decl fields[2]={{0}},args[3]={{0}},function={0},*functions[]={&function};
 Desc frame={0},data={0};uchar map[]={0,0xc0};Inst code[6];Dis64TypedBody p={0};int i;
 mod.kind=Tmodule;mod.size=mod.align=4;text.kind=Tstring;text.size=text.align=4;list.kind=Tlist;list.size=list.align=4;list.tof=&text;word.kind=Tint;word.size=word.align=4;
 fields[0].ty=&mod;fields[0].next=fields+1;fields[0].store=Dfield;fields[1].ty=&list;fields[1].offset=4;fields[1].store=Dfield;
 tuple.kind=Ttuple;tuple.ids=fields;tuple.size=8;tuple.align=4;fn.kind=Tfn;fn.tof=&tuple;fn.ids=args;
 args[0].ty=&mod;args[1].ty=&list;args[2].ty=&word;
 for(i=0;i<3;i++){args[i].store=Darg;args[i].offset=32+4*i;args[i].next=i<2?args+i+1:nil;}
 frame.id=1;frame.size=48;frame.map=map;frame.nmap=2;data.id=0;descriptors=&data;function.ty=&fn;function.desc=&frame;function.pc=code;
 paircode(code,3);paircheck(dis64typedloads(&function,nil,functions,1,code,3,3,&p)&&p.resultpair&&p.resultbytes==16&&p.returns[0]==8&&p.returns[1]==16,"both fields defined");dis64typedfree(&p);paircheck(!owners,"positive retirement");
 for(i=0;i<5;i++){
  paircode(code,3);
  if(i==0)pairscalar(code);if(i==1)pairscalar(code+1);if(i==2)pairstore(code+1,32,0);
  if(i==3)pairstore(code+1,32,4);if(i==4)pairstore(code+1,36,8);
  paircheck(!dis64typedloads(&function,nil,functions,1,code,3,3,&p),i==0?"missing first":i==1?"missing second":i==2?"duplicate first":i==3?"wrong field class":"third field");
  dis64typedfree(&p);paircheck(!owners,"failure retirement");
 }
 paircode(code,4);code[1].op=IJMP;code[1].sm=code[1].mm=Anone;code[1].dm=Apc;code[1].d.offset=3;code[1].branch=code+3;pairstore(code+2,36,4);
 paircheck(!dis64typedloads(&function,nil,functions,1,code,4,4,&p),"branch skips second");dis64typedfree(&p);
 paircode(code,6);code[0].op=IBEQW;code[0].sm=Aimm;code[0].s.offset=0;code[0].mm=Afp;code[0].m.reg=40;code[0].dm=Apc;code[0].d.offset=4;code[0].branch=code+4;
 pairstore(code+1,32,0);pairstore(code+2,36,4);pairstore(code+4,32,0);
 paircheck(!dis64typedloads(&function,nil,functions,1,code,6,6,&p),"one incomplete return path");dis64typedfree(&p);
 paircode(code,3);fields[0].ty=&list;fields[1].ty=&mod;
 paircheck(!dis64typedloads(&function,nil,functions,1,code,3,3,&p)&&!strcmp(p.reason,"aggregate-result"),"reversed pair refused");dis64typedfree(&p);
 fields[0].ty=&mod;fields[1].ty=&list;list.tof=&word;
 paircheck(!dis64typedloads(&function,nil,functions,1,code,3,3,&p),"list element class");dis64typedfree(&p);list.tof=&text;
 paircheck(!dis64typedexternal(&function,nil,functions,1,code,3,3,&p)&&!strcmp(p.reason,"aggregate-result"),"external profile closed");dis64typedfree(&p);
 paircheck(!owners,"all owners retired");puts("PASS pair IR: both roots defined on every return, missing/duplicate/class/branch refusals and old-profile boundary");return 0;
}
