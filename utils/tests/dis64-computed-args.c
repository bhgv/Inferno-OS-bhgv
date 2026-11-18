#include <stdio.h>
static void compcheck(int ok,char*why){if(!ok){fprintf(stderr,"computed argument IR: %s\n",why);exit(10);}}
int main(void){
 Type byte={0},word={0},text={0},array={0},list={0},any={0},module={0},callerfn={0},childfn={0};Decl arg={0},local={0},formal={0},caller={0},child={0},global={0},iface={0},*fns[]={&caller,&child};Desc cf={0},chf={0},data={0};uchar map[]={0,0x80},dm[]={0x80};Inst code[7]={{0}},saved;Dis64TypedBody p={0};int i;
 byte.kind=Tbyte;byte.size=byte.align=1;word.kind=Tint;word.size=word.align=4;text.kind=Tstring;text.size=text.align=4;array.kind=Tarray;array.tof=&byte;array.size=array.align=4;list.kind=Tlist;list.tof=&text;list.size=list.align=4;any.kind=Tany;any.size=any.align=4;module.kind=Tmodule;module.size=module.align=4;
 arg.ty=&array;arg.offset=32;arg.store=Darg;local.ty=&word;local.offset=36;local.store=Dlocal;formal.ty=&text;formal.offset=32;formal.store=Darg;global.ty=&any;global.store=Dglobal;
 cf.id=1;chf.id=2;cf.size=chf.size=40;cf.map=chf.map=map;cf.nmap=chf.nmap=2;data.size=4;data.map=dm;data.nmap=1;descriptors=&data;
 callerfn.kind=childfn.kind=Tfn;callerfn.tof=childfn.tof=&list;callerfn.ids=&arg;childfn.ids=&formal;caller.ty=&callerfn;caller.locals=&local;caller.desc=&cf;caller.pc=code;child.ty=&childfn;child.desc=&chf;child.pc=code+5;
 for(i=0;i<7;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<6?code+i+1:nil;}
 code[0].op=IFRAME;code[0].sm=Adesc;code[0].s.offset=2;code[0].dm=Afp;code[0].d.reg=36;
 code[1].op=ICVTAC;code[1].sm=Afp;code[1].s.decl=&arg;code[1].s.reg=32;code[1].dm=Afpind;code[1].d.reg=36;code[1].d.offset=32;
 code[2].op=ILEA;code[2].sm=Afpind;code[2].s.reg=16;code[2].dm=Afpind;code[2].d.reg=36;code[2].d.offset=16;
 code[3].op=ICALL;code[3].sm=Afp;code[3].s.reg=36;code[3].dm=Apc;code[3].d.offset=5;code[3].branch=code+5;code[4].op=IRET;
 code[5].op=IMOVP;code[5].sm=Amp;code[5].s.decl=&global;code[5].dm=Afpind;code[5].d.reg=16;code[6].op=IRET;
 compcheck(dis64typedloads(&caller,&global,fns,2,code,7,5,&p)&&p.nsites==1&&p.sites[0].inherited&&p.code[1].source==64&&p.code[1].destination==64&&p.returns[1]==64,"complete local byte conversion");dis64typedfree(&p);
 compcheck(!dis64typedmulti(&caller,&global,fns,2,code,7,5,&p),"old local mode closed");dis64typedfree(&p);saved=code[1];
 for(i=0;i<8;i++){code[1]=saved;if(i==0)array.tof=&word;if(i==1)code[1].s.reg=36;if(i==2)code[1].s.offset=4;if(i==3)code[1].sm=Afpind;if(i==4)code[1].d.offset=36;if(i==5)code[1].d.reg=32;if(i==6)code[1].mm=Afp;if(i==7)code[1].branch=code+4;
 compcheck(!dis64typedloads(&caller,&global,fns,2,code,7,5,&p),i==0?"wrong array element refused":"malformed source/frame/branch refused");dis64typedfree(&p);array.tof=&byte;}
 compcheck(!owners,"all compiler owners retired");puts("PASS computed argument IR: exact local String conversion, inherited result, eight class/address/setup refusals and legacy boundary");return 0;
}
