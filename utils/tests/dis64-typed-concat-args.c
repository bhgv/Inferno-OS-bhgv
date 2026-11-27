#include <stdio.h>
static void concatcheck(int ok,char *why){if(!ok){fprintf(stderr,"concat IR predicate: %s\n",why);exit(10);}}
int main(void){
 Type text={0},module={0},word={0},any={0},list={0},callerfn={0},importfn={0};Decl args[4]={{0}},formal[2]={{0}},locals[2]={{0}},global={0},caller={0},import={0},iface={0},*functions[]={&caller};
 Desc frame={0},data={0};uchar map[]={0,0xe8},dmap[]={0x80};Inst code[6]={{0}},saved;Dis64TypedBody p={0};int i;
 text.kind=Tstring;module.kind=Tmodule;word.kind=Tint;any.kind=Tany;any.size=any.align=4;list.kind=Tlist;list.tof=&text;
 text.size=module.size=word.size=list.size=text.align=module.align=word.align=list.align=4;
 callerfn.kind=importfn.kind=Tfn;callerfn.ids=args;callerfn.tof=importfn.tof=&text;importfn.ids=formal;
 for(i=0;i<4;i++){args[i].ty=i==0?&module:i==3?&word:&text;args[i].store=Darg;args[i].offset=32+4*i;args[i].next=i<3?args+i+1:nil;}
 formal[0].ty=&text;formal[1].ty=&word;for(i=0;i<2;i++){formal[i].store=Darg;formal[i].offset=32+4*i;}formal[0].next=formal+1;
 locals[0].ty=&text;locals[0].offset=48;locals[0].next=locals+1;locals[1].ty=&word;locals[1].offset=52;locals[0].store=locals[1].store=Dlocal;
 global.ty=&text;global.store=Dglobal;frame.size=64;frame.map=map;frame.nmap=2;data.size=4;data.map=dmap;data.nmap=1;descriptors=&data;
 caller.ty=&callerfn;caller.locals=locals;caller.desc=&frame;caller.pc=code;import.ty=&importfn;import.iface=&iface;
 for(i=0;i<6;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<5?code+i+1:nil;}
 code[0].op=IMFRAME;code[0].sm=Afp;code[0].s.reg=32;code[0].mm=Aoff;code[0].m.decl=&import;code[0].dm=Afp;code[0].d.reg=52;
 code[1].op=IADDC;code[1].sm=Afp;code[1].s.reg=36;code[1].mm=Amp;code[1].m.reg=0;code[1].dm=Afpind;code[1].d.reg=52;code[1].d.offset=32;
 code[2].op=IMOVW;code[2].sm=Aimm;code[2].dm=Afpind;code[2].d.reg=52;code[2].d.offset=36;
 code[3].op=ILEA;code[3].sm=Afpind;code[3].s.reg=16;code[3].dm=Afpind;code[3].d.reg=52;code[3].d.offset=16;
 code[4].op=IMCALL;code[4].sm=Afp;code[4].s.reg=52;code[4].mm=Aoff;code[4].m.decl=&import;code[4].dm=Afp;code[4].d.reg=32;code[5].op=IRET;
 concatcheck(dis64typedloads(&caller,&global,functions,1,code,6,6,&p)&&p.nexternal==1&&p.code[1].source==72&&p.code[1].middle==0&&p.code[1].destination==64&&p.returns[1]==64,"exact imported concat argument");dis64typedfree(&p);
 saved=code[1];code[1].mm=Afp;code[1].m.reg=40;
 concatcheck(dis64typedloads(&caller,&global,functions,1,code,6,6,&p)&&p.code[1].middle==80,"FP middle relocated");dis64typedfree(&p);code[1]=saved;
 concatcheck(!dis64typedexternal(&caller,&global,functions,1,code,6,6,&p),"external-only profile closed");dis64typedfree(&p);
 for(i=0;i<10;i++){
  code[1]=saved;
  if(i==0)code[1].s.reg=32;if(i==1){code[1].mm=Afp;code[1].m.reg=44;}if(i==2)code[1].mm=Aimm;
  if(i==3)code[1].d.offset=36;if(i==4)code[1].d.reg=48;if(i==5)code[1].sm=Afpind;if(i==6)code[1].branch=code+5;if(i==7)formal[0].ty=&list;if(i==8)args[1].ty=&any;if(i==9)global.ty=&any;
  concatcheck(!dis64typedloads(&caller,&global,functions,1,code,6,6,&p),"wrong source/middle/destination/frame/element roles refused");dis64typedfree(&p);formal[0].ty=&text;args[1].ty=&text;global.ty=&text;
 }
 concatcheck(!owners,"compiler owners retired");puts("PASS concat argument IR: exact String classes, FP/MP relocation, ten malformed refusals and old-profile boundary");return 0;
}
