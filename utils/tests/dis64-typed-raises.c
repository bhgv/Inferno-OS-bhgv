#include <stdio.h>
static void raisecheck(int ok,char *why){if(!ok){fprintf(stderr,"raise IR predicate: %s\n",why);exit(10);}}
static void raisecode(Inst *p,int n){memset(p,0,5*sizeof(*p));for(int i=0;i<n;i++){p[i].pc=i;p[i].op=IRET;p[i].sm=p[i].mm=p[i].dm=Anone;p[i].next=i+1<n?p+i+1:nil;}}
static void raisestore(Inst*p,int source,int field){p->op=IMOVP;p->sm=Afp;p->s.reg=source;p->dm=Afpind;p->d.reg=16;p->d.offset=field;}
int main(void){
 Type text={0},mod={0},list={0},word={0},tuple={0},fn={0};Decl fields[2]={{0}},args[4]={{0}},function={0},*functions[]={&function};
 Desc frame={0},data={0};uchar map[]={0,0xe0};Inst code[5];Dis64TypedBody p={0};int i;
 text.kind=Tstring;mod.kind=Tmodule;list.kind=Tlist;word.kind=Tint;
 text.size=mod.size=list.size=word.size=text.align=mod.align=list.align=word.align=4;list.tof=&text;
 fields[0].ty=&mod;fields[0].store=Dfield;fields[0].next=fields+1;fields[1].ty=&list;fields[1].store=Dfield;fields[1].offset=4;
 tuple.kind=Ttuple;tuple.ids=fields;tuple.size=8;tuple.align=4;fn.kind=Tfn;fn.tof=&tuple;fn.ids=args;
 args[0].ty=&text;args[1].ty=&mod;args[2].ty=&list;args[3].ty=&word;
 for(i=0;i<4;i++){args[i].store=Darg;args[i].offset=32+4*i;args[i].next=i<3?args+i+1:nil;}
 frame.id=1;frame.size=56;frame.map=map;frame.nmap=2;data.id=0;descriptors=&data;function.ty=&fn;function.desc=&frame;function.pc=code;
 raisecode(code,5);code[0].op=IBEQW;code[0].sm=Aimm;code[0].mm=Afp;code[0].m.reg=44;code[0].dm=Apc;code[0].branch=code+4;code[0].d.offset=4;
 raisestore(code+1,36,0);raisestore(code+2,40,4);code[4].op=IRAISE;code[4].sm=Afp;code[4].s.reg=32;
 raisecheck(dis64typedloads(&function,nil,functions,1,code,5,5,&p)&&p.code[4].source==64,"raise terminates result path");dis64typedfree(&p);
 code[4].op=IRET;code[4].sm=Anone;
 raisecheck(!dis64typedloads(&function,nil,functions,1,code,5,5,&p)&&!strcmp(p.reason,"undefined-result"),"unwritten return refused");dis64typedfree(&p);
 raisecode(code,1);code[0].op=IRAISE;code[0].sm=Afp;code[0].s.reg=32;
 raisecheck(dis64typedloads(&function,nil,functions,1,code,1,1,&p),"raise-only pair function");dis64typedfree(&p);
 for(i=0;i<7;i++){
  raisecode(code,1);code[0].op=IRAISE;code[0].sm=Afp;code[0].s.reg=32;
  if(i==0)code[0].s.reg=36;if(i==1)code[0].s.reg=40;if(i==2)code[0].s.reg=44;
  if(i==3)code[0].sm=Aimm;if(i==4)code[0].sm=Afpind;if(i==5)code[0].dm=Afp;if(i==6)code[0].mm=Aimm;
  raisecheck(!dis64typedloads(&function,nil,functions,1,code,1,1,&p),"non-String or malformed role refused");dis64typedfree(&p);
 }
 fn.tof=&word;raisecode(code,1);code[0].op=IRAISE;code[0].sm=Afp;code[0].s.reg=32;
 raisecheck(!dis64typedlower(&function,nil,code,1,1,&p),"old scalar profile closed");dis64typedfree(&p);
 raisecheck(!dis64typedexternal(&function,nil,functions,1,code,1,1,&p),"external-only profile closed");dis64typedfree(&p);
 function.handler=1;raisecheck(!dis64typedloads(&function,nil,functions,1,code,1,1,&p),"handler-bearing source stays closed");dis64typedfree(&p);
 raisecheck(!owners,"all compiler owners retired");puts("PASS raise IR: typed String roles, terminal and pair-return CFG, seven malformed roles, old-profile/handler boundaries");return 0;
}
