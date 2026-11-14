/* Appended to the allocation-accounted complete-body fixture. */
int main(void)
{
    Type word={0},fn={0};Decl arg={0},v={0},caller={0},leaf={0},*functions[]={&caller,&leaf};
    Desc cd={0},ld={0},data={0};Inst code[8]={{0}},save;Dis64TypedBody p={0};unsigned int i;
    typedbase();word.kind=Tint;word.size=word.align=4;fn.kind=Tfn;fn.tof=&word;fn.ids=&arg;
    arg.store=Darg;arg.ty=&word;arg.offset=32;v.store=Dlocal;v.ty=&word;v.offset=36;
    cd.id=1;cd.size=48;ld.id=2;ld.size=40;descriptors=&data;
    caller.ty=leaf.ty=&fn;caller.desc=&cd;caller.locals=&v;leaf.desc=&ld;caller.pc=code;leaf.pc=code+6;
    for(i=0;i<8;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<7?code+i+1:nil;}
    code[0].op=IFRAME;code[0].sm=Adesc;code[0].s.offset=2;code[0].dm=Afp;code[0].d.reg=40;
    code[1].op=IMOVW;code[1].sm=Afp;code[1].s.reg=32;code[1].dm=Afpind;code[1].d.reg=40;code[1].d.offset=32;
    code[2].op=ILEA;code[2].sm=Afp;code[2].s.reg=36;code[2].dm=Afpind;code[2].d.reg=40;code[2].d.offset=16;
    code[3].op=ICALL;code[3].sm=Afp;code[3].s.reg=40;code[3].dm=Apc;code[3].d.offset=6;
    code[4].op=IADDW;code[4].sm=Aimm;code[4].s.offset=1;code[4].mm=Afp;code[4].m.reg=36;code[4].dm=Afpind;code[4].d.reg=16;
    code[5].op=IRET;code[6].op=IMOVW;code[6].sm=Afp;code[6].s.reg=32;code[6].dm=Afpind;code[6].d.reg=16;code[7].op=IRET;
    assert(dis64typedlocal(&caller,nil,functions,2,code,8,6,&p));
    assert(p.tail==2 && p.callend==3 && p.resultoffset==68 && p.resultcontract==4 && p.returns[2]==132);dis64typedfree(&p);
    code[2].s.reg=37;assert(!dis64typedlocal(&caller,nil,functions,2,code,8,6,&p));dis64typedfree(&p);code[2].s.reg=36;
    code[2].d.offset=20;assert(!dis64typedlocal(&caller,nil,functions,2,code,8,6,&p));dis64typedfree(&p);code[2].d.offset=16;
    save=code[4];code[4].op=IMOVW;code[4].mm=Anone;code[4].dm=Afp;code[4].d.reg=36;
    assert(!dis64typedlocal(&caller,nil,functions,2,code,8,6,&p) && !strcmp(p.reason,"undefined-result"));dis64typedfree(&p); /* LOCAL_RESULT_DEFINED */
    code[4]=save;code[4].op=IJMP;code[4].sm=code[4].mm=Anone;code[4].dm=Apc;code[4].d.offset=1;code[4].branch=code+1;
    assert(!dis64typedlocal(&caller,nil,functions,2,code,8,6,&p) && !strcmp(p.reason,"local-setup-branch"));dis64typedfree(&p); /* LOCAL_SETUP_BRANCH */
    code[4]=save;code[4].pc=3;
    assert(!dis64typedlocal(&caller,nil,functions,2,code,8,6,&p) && p.reason!=nil);dis64typedfree(&p);code[4].pc=4;
    assert(owners==0);return 0;
}
