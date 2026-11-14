/* Allocation-accounted trusted IR boundary; appended to the shared fixture. */
int main(void)
{
    Type word={0},module={0},callerfn={0},importfn={0};Decl args[2]={{0}},arg={0},locals[3]={{0}},caller={0},import={0},iface={0},other={0},*functions[]={&caller};
    Desc cd={0},data={0};uchar map[]={0,0x80};Inst code[11]={{0}},saved,bind;Dis64TypedBody p={0};unsigned i;
    typedbase();word.kind=Tint;word.size=word.align=4;module.kind=Tmodule;module.size=module.align=4;
    args[0].store=args[1].store=arg.store=Darg;args[0].ty=&module;args[0].offset=32;args[0].next=args+1;args[1].ty=&word;args[1].offset=36;
    arg.ty=&word;arg.offset=32;callerfn.kind=importfn.kind=Tfn;callerfn.ids=args;importfn.ids=&arg;callerfn.tof=importfn.tof=&word;
    import.ty=&importfn;import.iface=&iface;iface.offset=2;other=import;
    for(i=0;i<3;i++){locals[i].store=Dlocal;locals[i].ty=&word;locals[i].offset=40+4*i;locals[i].next=i<2?locals+i+1:nil;}
    cd.size=56;cd.map=map;cd.nmap=2;descriptors=&data;caller.ty=&callerfn;caller.desc=&cd;caller.locals=locals;caller.pc=code;
    for(i=0;i<11;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<10?code+i+1:nil;}
    code[0].op=IMOVW;code[0].sm=Aimm;code[0].s.offset=3;code[0].dm=Afp;code[0].d.reg=36;
    for(i=1;i<=5;i+=4){
        code[i].op=IMFRAME;code[i].sm=Afp;code[i].s.reg=32;code[i].mm=Aoff;code[i].m.decl=&import;code[i].dm=Afp;code[i].d.reg=48;
        code[i+1].op=IMOVW;code[i+1].sm=Afp;code[i+1].s.reg=36;code[i+1].dm=Afpind;code[i+1].d.reg=48;code[i+1].d.offset=32;
        code[i+2].op=ILEA;code[i+2].sm=Afp;code[i+2].s.reg=i==1?40:44;code[i+2].dm=Afpind;code[i+2].d.reg=48;code[i+2].d.offset=16;
        code[i+3].op=IMCALL;code[i+3].sm=Afp;code[i+3].s.reg=48;code[i+3].mm=Aoff;code[i+3].m.decl=&import;code[i+3].dm=Afp;code[i+3].d.reg=32;
    }
    code[9].op=IADDW;code[9].sm=Afp;code[9].s.reg=40;code[9].mm=Afp;code[9].m.reg=44;code[9].dm=Afpind;code[9].d.reg=16;code[10].op=IRET;
    assert(dis64typedexternal(&caller,nil,functions,1,code,11,11,&p));assert(p.tail==4 && p.nexternal==2 && p.external[1].binding==2 && p.external[0].moduleoffset==64);dis64typedfree(&p);
    code[4].d.reg=36;assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p));dis64typedfree(&p);code[4].d.reg=32; /* EXTERNAL_MODULE */
    code[6].d.offset=36;assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p));dis64typedfree(&p);code[6].d.offset=32; /* EXTERNAL_ARGUMENT */
    iface.offset=32768;assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p));dis64typedfree(&p);iface.offset=2; /* EXTERNAL_ORDINAL */
    code[4].m.decl=&other;assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p));dis64typedfree(&p);code[4].m.decl=&import;
    importfn.varargs=1;assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p));dis64typedfree(&p);importfn.varargs=0;
    saved=code[0];code[0].op=IJMP;code[0].sm=Anone;code[0].dm=Apc;code[0].branch=code+2;code[0].d.offset=2;
    assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p) && !strcmp(p.reason,"multi-setup-branch"));dis64typedfree(&p);code[0]=saved;
    bind=code[7];saved=code[9];code[7].sm=Afpind;code[7].s.reg=16;
    code[9].op=IMOVW;code[9].sm=Aimm;code[9].s.offset=0;code[9].mm=Anone;code[9].dm=Afp;code[9].d.reg=36;
    assert(dis64typedexternal(&caller,nil,functions,1,code,11,11,&p) && p.external[1].inherited && p.returns[8]==32);dis64typedfree(&p); /* EXTERNAL_INHERITED */
    code[7]=bind;assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p) && !strcmp(p.reason,"undefined-result"));dis64typedfree(&p);code[9]=saved;
    module.kind=Tref;assert(!dis64typedexternal(&caller,nil,functions,1,code,11,11,&p));dis64typedfree(&p);module.kind=Tmodule;
    assert(!dis64typedmulti(&caller,nil,functions,1,code,11,11,&p));dis64typedfree(&p);assert(owners==0);return 0;
}
