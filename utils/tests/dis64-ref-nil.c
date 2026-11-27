#include <stdio.h>
static void refcheck(int ok,char*why){if(!ok){fprintf(stderr,"ref-nil predicate: %s\n",why);exit(10);}}
int main(void){
 Type adt={0},ref={0},any={0},none={0},fn={0};Decl arg={0},empty={0},function={0},*functions[]={&function};Desc frame={0},data={0};uchar fm[]={0,0x80},dm[]={0x80};Inst code[2]={{0}},saved;Dis64TypedBody p={0};int k;
 adt.kind=Tadt;ref.kind=Tref;ref.tof=&adt;any.kind=Tany;none.kind=Tnone;ref.size=any.size=ref.align=any.align=4;
 fn.kind=Tfn;fn.ids=&arg;fn.tof=&none;arg.ty=&ref;arg.offset=32;arg.store=Darg;empty.ty=&any;empty.store=Dglobal;empty.offset=0;
 frame.id=1;frame.size=40;frame.map=fm;frame.nmap=2;data.size=4;data.map=dm;data.nmap=1;descriptors=&data;function.ty=&fn;function.desc=&frame;function.pc=code;
 code[0].pc=0;code[0].op=IBEQW;code[0].sm=Afp;code[0].s.decl=&arg;code[0].s.reg=32;code[0].mm=Amp;code[0].m.decl=&empty;code[0].dm=Apc;code[0].d.offset=1;code[0].branch=code+1;code[0].next=code+1;
 code[1].pc=1;code[1].op=IRET;code[1].sm=code[1].mm=code[1].dm=Anone;
 for(k=0;k<4;k++){code[0].op=k&1?IBNEW:IBEQW;if(k==2){Addr a=code[0].s;code[0].s=code[0].m;code[0].m=a;code[0].sm=Amp;code[0].mm=Afp;}
 refcheck(dis64typedloads(&function,&empty,functions,1,code,2,2,&p),"direct root accepted");refcheck(p.code[0].op==(k&1?IBNEL:IBEQL)&&p.code[0].source==64&&p.code[0].middle==0&&p.code[0].destination==1,"native long branch");dis64typedfree(&p);}
 code[0].sm=Afp;code[0].mm=Amp;code[0].s.decl=&arg;code[0].s.reg=32;code[0].m.decl=&empty;code[0].m.reg=0;saved=code[0];
 for(k=0;k<11;k++){code[0]=saved;
 if(k==0)code[0].s.offset=4;
 if(k==1)code[0].s.reg=36;
 if(k==2)code[0].sm=Afpind;
 if(k==3)code[0].mm=Afp;
 if(k==4)code[0].m.decl=&arg;
 if(k==5)code[0].d.offset=0;
 if(k==6)code[0].branch=nil;
 if(k==7)ref.tof=nil;
 if(k==8)fm[1]=0;
 if(k==9)dm[0]=0;
 if(k==10)ref.tof=&any;
 refcheck(!dis64typedloads(&function,&empty,functions,1,code,2,2,&p),k==0?"field offset refused":"malformed root/nil/branch refused");dis64typedfree(&p);ref.tof=&adt;fm[1]=0x80;dm[0]=0x80;}
 refcheck(!owners,"all owners retired");puts("PASS direct ref/nil IR: both orders/equalities, eleven field/role/map/branch refusals, complete allocation retirement");return 0;
}
