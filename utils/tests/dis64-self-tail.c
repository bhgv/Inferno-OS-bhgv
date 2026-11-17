#include <stdio.h>
static void selfcheck(int ok,char*why){if(!ok){fprintf(stderr,"self-tail predicate: %s\n",why);exit(10);}}
int main(void){
 Type text={0},list={0},word={0},fn={0};Decl arg={0},local={0},function={0},*functions[]={&function};Desc frame={0},data={0};uchar map[]={0,0x80};Inst code[6]={{0}},saved;Dis64TypedBody p={0};int i;
 text.kind=Tstring;list.kind=Tlist;list.tof=&text;word.kind=Tint;text.size=list.size=word.size=text.align=list.align=word.align=4;
 fn.kind=Tfn;fn.ids=&arg;fn.tof=&list;arg.ty=&text;arg.offset=32;arg.store=Darg;local.ty=&word;local.offset=36;local.store=Dlocal;
 frame.id=1;frame.size=40;frame.map=map;frame.nmap=2;descriptors=&data;function.ty=&fn;function.locals=&local;function.desc=&frame;function.pc=code;
 for(i=0;i<6;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<4?code+i+1:nil;}
 code[0].op=IFRAME;code[0].sm=Adesc;code[0].s.offset=1;code[0].dm=Afp;code[0].d.reg=36;
 code[1].op=IMOVP;code[1].sm=Afp;code[1].s.reg=32;code[1].dm=Afpind;code[1].d.reg=36;code[1].d.offset=32;
 code[2].op=ILEA;code[2].sm=Afpind;code[2].s.reg=16;code[2].dm=Afpind;code[2].d.reg=36;code[2].d.offset=16;
 code[3].op=ICALL;code[3].sm=Afp;code[3].s.reg=36;code[3].dm=Apc;code[3].branch=code;
 code[4].op=IRET;code[5].op=IRET;
 selfcheck(dis64typedloads(&function,nil,functions,1,code,5,5,&p)&&p.nsites==1&&p.sites[0].inherited&&p.sites[0].contract==24,"canonical self tail");dis64typedfree(&p);
 selfcheck(!dis64typedmulti(&function,nil,functions,1,code,5,5,&p),"old multi profile closed");dis64typedfree(&p);
 code[4].op=INOP;code[4].next=code+5;
 selfcheck(!dis64typedloads(&function,nil,functions,1,code,6,6,&p),"non-adjacent return refused");dis64typedfree(&p);code[4].op=IRET;code[4].next=nil;
 saved=code[2];code[2].s.reg=20;selfcheck(!dis64typedloads(&function,nil,functions,1,code,5,5,&p),"wrong result slot");dis64typedfree(&p);code[2]=saved;
 code[0].s.offset=2;selfcheck(!dis64typedloads(&function,nil,functions,1,code,5,5,&p),"wrong descriptor");dis64typedfree(&p);code[0].s.offset=1;
 code[3].d.offset=1;selfcheck(!dis64typedloads(&function,nil,functions,1,code,5,5,&p),"wrong entry");dis64typedfree(&p);code[3].d.offset=0;
 map[1]=0;selfcheck(!dis64typedloads(&function,nil,functions,1,code,5,5,&p),"missing argument root");dis64typedfree(&p);
 selfcheck(!owners,"all compiler owners retired");puts("PASS checked self-tail: complete frame/result identity, canonical tail, old-profile and malformed IR refusals");return 0;
}
