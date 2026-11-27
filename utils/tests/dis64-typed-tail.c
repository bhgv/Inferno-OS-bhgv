/* Appended to the existing allocation-accounted typed-body core fixture. */
int main(void)
{
    Type word={0},fn={0};Decl arg={0},caller={0},leaf={0},*functions[]={&caller,&leaf};
    Desc cd={0},ld={0},data={0};Inst code[7]={{0}};Dis64TypedBody p={0};unsigned int i;
    typedbase();
    word.kind=Tint;word.size=word.align=4;fn.kind=Tfn;fn.tof=&word;fn.ids=&arg;
    arg.store=Darg;arg.ty=&word;arg.offset=32;cd.id=1;ld.id=2;cd.size=ld.size=40;descriptors=&data;
    caller.ty=leaf.ty=&fn;caller.desc=&cd;leaf.desc=&ld;caller.pc=code;leaf.pc=code+5;
    for(i=0;i<7;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<6?code+i+1:nil;}
    code[0].op=IFRAME;code[0].sm=Adesc;code[0].s.offset=2;code[0].dm=Afp;code[0].d.reg=36;
    code[1].op=IMOVW;code[1].sm=Afp;code[1].s.reg=32;code[1].dm=Afpind;code[1].d.reg=36;code[1].d.offset=32;
    code[2].op=ILEA;code[2].sm=Afpind;code[2].s.reg=16;code[2].dm=Afpind;code[2].d.reg=36;code[2].d.offset=16;
    code[3].op=ICALL;code[3].sm=Afp;code[3].s.reg=36;code[3].dm=Apc;code[3].d.offset=5;code[4].op=IRET;
    code[5].op=IMOVW;code[5].sm=Afp;code[5].s.reg=32;code[5].dm=Afpind;code[5].d.reg=16;code[6].op=IRET;
    assert(dis64typedtail(&caller,nil,functions,2,code,7,5,&p));
    assert(p.tail && p.target==5 && p.targetdesc==2 && p.code[1].destination==64 && p.code[2].source==32 && p.returns[2]==64);dis64typedfree(&p);
    code[0].s.offset=999;assert(!dis64typedtail(&caller,nil,functions,2,code,7,5,&p));dis64typedfree(&p);code[0].s.offset=2; /* TAIL_DESCRIPTOR */
    code[2].s.reg=20;assert(!dis64typedtail(&caller,nil,functions,2,code,7,5,&p));dis64typedfree(&p);code[2].s.reg=16; /* TAIL_BINDING */
    code[1].d.offset=36;assert(!dis64typedtail(&caller,nil,functions,2,code,7,5,&p));dis64typedfree(&p);code[1].d.offset=32; /* TAIL_ARGUMENT */
    code[3].s.reg=40;assert(!dis64typedtail(&caller,nil,functions,2,code,7,5,&p));dis64typedfree(&p);code[3].s.reg=36;
    code[3].d.offset=1;assert(!dis64typedtail(&caller,nil,functions,2,code,7,5,&p));dis64typedfree(&p);code[3].d.offset=5;
    code[3].d.offset=0;code[0].s.offset=1;
    assert(!dis64typedtail(&caller,nil,functions,2,code,7,5,&p) && !strcmp(p.reason,"tail-callee"));dis64typedfree(&p);
    code[3].d.offset=5;code[0].s.offset=2;
    code[1].op=IMOVP;assert(!dis64typedtail(&caller,nil,functions,2,code,7,5,&p));dis64typedfree(&p);code[1].op=IMOVW;
    assert(owners==0);
    return 0;
}
