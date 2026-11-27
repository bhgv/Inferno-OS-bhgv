/* Appended to the allocation-accounted complete-body fixture. */
int main(void)
{
    Type word={0},fn={0};Decl arg={0},locals[3]={{0}},caller={0},leaf={0},*functions[]={&caller,&leaf};
    Desc cd={0},ld={0},data={0};Inst code[13]={{0}},save,bind;Dis64TypedBody p={0};unsigned int i;
    typedbase();word.kind=Tint;word.size=word.align=4;fn.kind=Tfn;fn.tof=&word;fn.ids=&arg;
    arg.store=Darg;arg.ty=&word;arg.offset=32;
    for(i=0;i<3;i++){locals[i].store=Dlocal;locals[i].ty=&word;locals[i].offset=36+4*i;locals[i].next=i<2?locals+i+1:nil;}
    cd.id=1;cd.size=56;ld.id=2;ld.size=40;descriptors=&data;
    caller.ty=leaf.ty=&fn;caller.desc=&cd;caller.locals=locals;leaf.desc=&ld;caller.pc=code;leaf.pc=code+11;
    for(i=0;i<13;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<12?code+i+1:nil;}
    code[0].op=IMOVW;code[0].sm=Aimm;code[0].s.offset=3;code[0].dm=Afp;code[0].d.reg=36;
    for(i=1;i<=5;i+=4){
        code[i].op=IFRAME;code[i].sm=Adesc;code[i].s.offset=2;code[i].dm=Afp;code[i].d.reg=48;
        code[i+1].op=IMOVW;code[i+1].sm=Afp;code[i+1].s.reg=i==1?36:32;code[i+1].dm=Afpind;code[i+1].d.reg=48;code[i+1].d.offset=32;
        code[i+2].op=ILEA;code[i+2].sm=Afp;code[i+2].s.reg=i==1?40:44;code[i+2].dm=Afpind;code[i+2].d.reg=48;code[i+2].d.offset=16;
        code[i+3].op=ICALL;code[i+3].sm=Afp;code[i+3].s.reg=48;code[i+3].dm=Apc;code[i+3].d.offset=11;
    }
    code[9].op=IADDW;code[9].sm=Afp;code[9].s.reg=40;code[9].mm=Afp;code[9].m.reg=44;code[9].dm=Afpind;code[9].d.reg=16;
    code[10].op=IRET;code[11].op=IMOVW;code[11].sm=Afp;code[11].s.reg=32;code[11].dm=Afpind;code[11].d.reg=16;code[12].op=IRET;
    assert(dis64typedmulti(&caller,nil,functions,2,code,13,11,&p));
    assert(p.tail==3 && p.nsites==2 && p.sites[0].first==1 && p.sites[1].end==8 && p.sites[1].resultoffset==76);dis64typedfree(&p);
    code[5].s.offset=999;assert(!dis64typedmulti(&caller,nil,functions,2,code,13,11,&p));dis64typedfree(&p);code[5].s.offset=2; /* MULTI_DESCRIPTOR */
    code[6].d.offset=36;assert(!dis64typedmulti(&caller,nil,functions,2,code,13,11,&p));dis64typedfree(&p);code[6].d.offset=32; /* MULTI_ARGUMENT */
    save=code[0];code[0].op=IJMP;code[0].sm=Anone;code[0].dm=Apc;code[0].d.offset=2;code[0].branch=code+2;
    assert(!dis64typedmulti(&caller,nil,functions,2,code,13,11,&p) && !strcmp(p.reason,"multi-setup-branch"));dis64typedfree(&p); /* MULTI_BRANCH */
    code[0]=save;
    save=code[9];bind=code[7];code[7].sm=Afpind;code[7].s.reg=16;code[7].s.offset=0;
    code[9].op=IMOVW;code[9].sm=Aimm;code[9].s.offset=0;code[9].mm=Anone;code[9].dm=Afp;code[9].d.reg=36;
    assert(dis64typedmulti(&caller,nil,functions,2,code,13,11,&p) && p.sites[1].inherited && p.returns[8]==32);dis64typedfree(&p); /* MULTI_INHERITED */
    code[7]=bind;
    assert(!dis64typedmulti(&caller,nil,functions,2,code,13,11,&p) && !strcmp(p.reason,"undefined-result"));dis64typedfree(&p);code[9]=save;
    code[4].d.offset=0;code[1].s.offset=1;
    assert(!dis64typedmulti(&caller,nil,functions,2,code,13,11,&p));dis64typedfree(&p);code[4].d.offset=11;code[1].s.offset=2;
    code[12].pc=11;assert(!dis64typedmulti(&caller,nil,functions,2,code,13,11,&p));dis64typedfree(&p);code[12].pc=12;
    assert(!dis64typedmulti(&caller,nil,functions,0,code,13,11,&p));dis64typedfree(&p);
    assert(!dis64typedmulti(&caller,nil,functions,2,code,65537,11,&p));dis64typedfree(&p);
    assert(owners==0);return 0;
}
