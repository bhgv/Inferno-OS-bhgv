#include <stdio.h>
static void varircheck(int ok,char*why){if(!ok){fprintf(stderr,"variadic IR: %s\n",why);exit(10);}}
int main(void){
 Type text={0},module={0},word={0},fn={0},actual={0},callerfn={0},ref={0};Decl formal={0},arg={0},temp={0},scratch={0},global={0},caller={0},callee={0},parent={0},iface={0},frameDecl={0},*fns[]={&caller};
 Sym sys={0},name={0},tmpname={0};Desc cd={0},fd={0},data={0};uchar cm[]={0,0xc0},fm[]={0,0x80},dm[]={0x80};Inst code[6]={{0}},saved;Dis64TypedBody p={0};int i;
 text.kind=Tstring;text.size=text.align=4;module.kind=Tmodule;module.size=module.align=4;word.kind=Tint;word.size=word.align=4;ref.kind=Tref;ref.size=ref.align=4;
 formal.ty=&text;formal.store=Dfield;formal.offset=32;actual.kind=fn.kind=callerfn.kind=Tfn;actual.varargs=fn.varargs=1;actual.ids=fn.ids=&formal;actual.tof=fn.tof=callerfn.tof=&text;fn.sig=0x4c0624b6U;
 sys.name="Sys";sys.len=3;name.name="sprint";name.len=6;tmpname.name=".t0";tmpname.len=3;parent.sym=&sys;callee.sym=&name;callee.dot=&parent;callee.ty=&fn;callee.iface=&iface;
 arg.ty=&module;arg.offset=32;arg.store=Darg;temp.ty=&text;temp.offset=36;temp.store=Dlocal;temp.sym=&tmpname;temp.next=&scratch;scratch.ty=&word;scratch.offset=40;scratch.store=Dlocal;global.ty=&text;global.store=Dglobal;
 cd.id=1;cd.size=48;cd.map=cm;cd.nmap=2;fd.id=2;fd.size=36;fd.map=fm;fd.nmap=2;data.size=4;data.map=dm;data.nmap=1;descriptors=&data;frameDecl.desc=&fd;
 callerfn.ids=&arg;caller.ty=&callerfn;caller.locals=&temp;caller.desc=&cd;caller.pc=code;
 for(i=0;i<6;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<5?code+i+1:nil;}
 code[0].op=IFRAME;code[0].sm=Adesc;code[0].s.decl=&frameDecl;code[0].s.offset=2;code[0].dm=Afp;code[0].d.reg=40;code[0].native64args=&actual;
 code[1].op=IMOVP;code[1].sm=Amp;code[1].s.decl=&global;code[1].dm=Afpind;code[1].d.reg=40;code[1].d.offset=32;
 code[2].op=ILEA;code[2].sm=Afp;code[2].s.reg=36;code[2].s.decl=&temp;code[2].dm=Afpind;code[2].d.reg=40;code[2].d.offset=16;
 code[3].op=IMCALL;code[3].sm=Afp;code[3].s.reg=40;code[3].mm=Aoff;code[3].m.decl=&callee;code[3].dm=Afp;code[3].d.reg=32;code[3].d.decl=&arg;
 code[4].op=IMOVP;code[4].sm=Afp;code[4].s.reg=36;code[4].s.decl=&temp;code[4].dm=Afpind;code[4].d.reg=16;code[5].op=IRET;
 varircheck(dis64typedloads(&caller,&global,fns,1,code,6,6,&p)&&p.nvariadic==1&&p.variadic[0].frame.bytes==72&&p.variadic[0].count==0,"complete empty-tail sprint");dis64typedfree(&p);
 varircheck(!dis64typedexternal(&caller,&global,fns,1,code,6,6,&p),"ordinary profile closed");dis64typedfree(&p);
 for(i=0;i<10;i++){
  saved=code[i==0?0:i<4?1:2];
  if(i==0)code[0].native64args=nil;
  if(i==1)code[1].sm=Aimm;
  if(i==2)code[1].d.offset=36;
  if(i==3)code[1].branch=code+4;
  if(i==4)code[2].s.reg=40;
  if(i==5)code[2].d.offset=20;
  if(i==6)fn.sig^=1;
  if(i==7)fm[1]=0;
  if(i==8)formal.ty=&word;
  if(i==9)code[2].mm=Afp;
  varircheck(!dis64typedloads(&caller,&global,fns,1,code,6,6,&p),i==6?"signature refused":"malformed annotation/map/argument/result refused");dis64typedfree(&p);
  code[i==0?0:i<4?1:2]=saved;fn.sig=0x4c0624b6U;fm[1]=0x80;formal.ty=&text;
 }
 /* The actual per-use type refines only compiler temporary roots in LOAD. */
 code[0].op=IMFRAME;code[0].native64args=nil;code[0].sm=Afp;code[0].s.reg=32;code[0].s.decl=&arg;code[0].mm=Aoff;code[0].m.decl=&callee;
 fn.varargs=0;fn.ids=nil;code[1]=code[2];code[1].pc=1;code[1].next=code+2;code[2]=code[3];code[2].pc=2;code[2].next=code+3;code[3]=code[4];code[3].pc=3;code[3].next=code+4;code[4]=code[5];code[4].pc=4;code[4].next=nil;
 temp.ty=&ref;code[1].s.native64type=code[3].s.native64type=&text;
 varircheck(dis64typedloads(&caller,&global,fns,1,code,5,5,&p),"reused managed temporary per-use String type");dis64typedfree(&p);
 varircheck(!dis64typedexternal(&caller,&global,fns,1,code,5,5,&p),"temporary refinement LOAD-only");dis64typedfree(&p);
 tmpname.name="named";varircheck(!dis64typedloads(&caller,&global,fns,1,code,5,5,&p),"ordinary local cannot refine nominal root type");dis64typedfree(&p);tmpname.name=".t0";
 code[1].s.native64type=&word;varircheck(!dis64typedloads(&caller,&global,fns,1,code,5,5,&p),"scalar annotation cannot refine a root");dis64typedfree(&p);
 varircheck(!owners,"all compiler owners retired");puts("PASS variadic IR: exact instantiated layout, ten annotation/hash/map/operand refusals, temporary per-use types and old-profile closure");return 0;
}
