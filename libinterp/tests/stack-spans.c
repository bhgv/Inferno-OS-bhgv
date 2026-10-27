
static union {
    uvlong align; // Alignment for naturally allocated root extents and Type prefixes.
    uchar bytes[512]; // Source, current frame and shifted Type storage with full canaries.
} spancurrent, spansource, spanroot, spantypes, spanslot;
static Type spanvalid, spanbad; // Independent current-frame and source descriptors.
static Module spanmodule; // Stack hint owner, retained across the call.
static Modlink spanlink; // Borrowed module instance used by newstack and extend.
static Prog spanchild; // Unpublished destination whose complete REG is checked.
static Frame *spansrc; // Borrowed source frame selected before native error protection.
static Prog *spandestination; // Checked destination owner, including invalid-prefix probes.
static int spanoperation; // Public operation, newstack/extend/unframe/unextend.


/* Initialize a complete private source and preserve independent caller metadata. */
static void
spanreset(int operation,int framealign,int slotalign,int root)
{
    Stkext *sx=(Stkext*)spanroot.bytes;

    memset(&R,0,sizeof(R));memset(&spanchild,0,sizeof(spanchild));
    memset(&spanvalid,0,sizeof(spanvalid));memset(&spanbad,0,sizeof(spanbad));
    memset(&spanmodule,0,sizeof(spanmodule));memset(&spanlink,0,sizeof(spanlink));
    memset(spancurrent.bytes,0x5a,sizeof(spancurrent.bytes));
    memset(spansource.bytes,0x67,sizeof(spansource.bytes));
    memset(spanroot.bytes,0x39,sizeof(spanroot.bytes));
    spanvalid.size=spanbad.size=sizeof(Frame)+32;
    spanlink.m=&spanmodule;R.M=spanchild.R.M=&spanlink;
    putframe(spancurrent.bytes+16+framealign,&spanvalid);R.FP=spancurrent.bytes+16+framealign;
    if(root){
        sx->reg.TR=&spanbad;sx->reg.SP=(uchar*)4;sx->reg.TS=(uchar*)5;sx->reg.EX=(uchar*)6;
        putframe(sx->reg.tos.fu,nil);spansrc=sx->reg.tos.fr;
    }else{
        putframe(spansource.bytes+16+framealign,&spanbad);spansrc=(Frame*)(spansource.bytes+16+framealign);
    }
    memmove(spanslot.bytes+16+slotalign,&spansrc,sizeof(spansrc));
    R.s=operation==1 ? (void*)&spanbad : (void*)(spanslot.bytes+16+slotalign);
    R.SP=(uchar*)1;R.TS=(uchar*)2;R.EX=(uchar*)3;
    allocations=releases=failure=0;raised=nil;spantype=nil;spanregisters=0;
    spandestination=&spanchild;spanoperation=operation;
}


/* Invoke production operations without changing their source or error semantics. */
static void
spancall(void)
{
    switch(spanoperation){
    case 0:newstack(spandestination);break;
    case 1:extend();break;
    case 2:unframe();break;
    default:unextend(spansrc);break;
    }
}


/* Check complete state, no allocation/release and immutable inputs on rejection. */
static void
spanreject(char *wanted)
{
    REG before, childbefore;
    uchar current[512],source[512],root[512];

    memmove(&before,&R,sizeof(R));memmove(&childbefore,&spanchild.R,sizeof(R));
    memmove(current,spancurrent.bytes,sizeof(current));
    memmove(source,spansource.bytes,sizeof(source));memmove(root,spanroot.bytes,sizeof(root));
    if(setjmp(boundary)==0){spancall();spancheck(0);}
    spancheck(raised==wanted && allocations==0 && releases==0 && owned==nil);
    spancheck(!memcmp(&before,&R,sizeof(R)) && !memcmp(&childbefore,&spanchild.R,sizeof(R)));
    spancheck(!memcmp(current,spancurrent.bytes,sizeof(current)));
    spancheck(!memcmp(source,spansource.bytes,sizeof(source)) && !memcmp(root,spanroot.bytes,sizeof(root)));
    checks++;
}


/* Reject every mapped Frame link plus invalid map lengths and partial pointer slots. */
static void
spanmaps(void)
{
    int op,kind,a,b,root;

    for(op=0;op<4;op++)for(kind=0;kind<9;kind++)for(a=0;a<8;a++)for(b=0;b<8;b++)for(root=0;root<2;root++){
        if(op==3 && !root)continue;
        spanreset(op,a,b,root);
        spanbad.np=1;
        if(kind<4)spanbad.map[0]=0x80>>kind;
        if(kind==4)spanbad.np=-1;
        if(kind==5)spanbad.np=2;
        if(kind==6){spanbad.size=sizeof(Frame);spanbad.map[0]=1;}
        if(kind==7){spanbad.size=sizeof(Frame)+1;spanbad.map[0]=0x80>>(sizeof(Frame)/sizeof(void*));}
        if(kind==8)spanbad.size=-1;
        spanreject(exHeap);
    }
}


/* Bound decoded slots, frame headers, descriptor prefixes and backward root recovery. */
static void
spangeometry(void)
{
    uintptr i;
    int op;
    Type *type;
    Frame *frame;

    for(op=0;op<4;op++){
        for(i=0;i<sizeof(Frame);i++){
            if(op==1)continue;
            spanreset(op,0,0,op==3);
            frame=(Frame*)((uintptr)-1-i);
            if(op==2)R.FP=(uchar*)frame;
            else if(op==3)spansrc=frame;
            else memmove(R.s,&frame,sizeof(frame));
            spanreject(i==0 ? exNilref : exHeap);
        }
        for(i=0;i<sizeof(void*)+2;i++){
            if(op==1 || op==3)continue;
            spanreset(op,0,0,0);
            R.s=i==0 ? nil : (void*)((uintptr)0-(i-1));
            if(i==1)R.s=H;
            spanreject(exBounds);
        }
        for(i=1;i<sizeof(void*);i++){
            spanreset(op,0,0,op==3);
            type=(Type*)(spantypes.bytes+i);
            if(op==1)R.s=type;
            else if(op==3)((Stkext*)spanroot.bytes)->reg.TR=type;
            else memmove((uchar*)spansrc+O(Frame,t),&type,sizeof(type));
            spanreject(exHeap);
        }
    }
    for(op=0;op<4;op++)for(i=0;i<O(Type,np)+sizeof(spanbad.np);i++){
        spanreset(op,0,0,op==3);
        type=(Type*)((uintptr)-1-i);
        if(op==1)R.s=type;
        else if(op==3)((Stkext*)spanroot.bytes)->reg.TR=type;
        else memmove((uchar*)spansrc+O(Frame,t),&type,sizeof(type));
        spanreject(exHeap);
    }
    for(i=0;i<sizeof(Prog);i++){
        spanreset(0,0,0,0);spandestination=(Prog*)((uintptr)-1-i);spanreject(exHeap);
    }
    spanreset(3,0,0,0);spanreject(exHeap);
    for(i=1;i<OA(Stkext,reg.tos.fu);i++){
        spanreset(3,0,0,1);spansrc=(Frame*)i;spanreject(exHeap);
    }
    for(i=0;i<sizeof(Modlink);i++){
        spanreset(1,0,0,0);R.M=(Modlink*)((uintptr)-1-i);spanreject(exModule);
    }
    for(i=0;i<sizeof(Module);i++){
        spanreset(0,0,0,0);spanlink.m=(Module*)((uintptr)-1-i);spanreject(exModule);
    }
}


/* Allocation observers must not change a captured frame size or saved parent cursors. */
static void
spancapture(void)
{
    static const int sizes[]={sizeof(Frame),sizeof(Frame)+32,256,512}; // Values injected only after allocation sizing.
    Stkext *sx;
    REG before;
    int op,a,b,size;

    for(op=0;op<2;op++)for(a=0;a<8;a++)for(b=0;b<8;b++)for(size=0;size<4;size++){
        spanreset(op,a,b,0);memmove(&before,&R,sizeof(R));
        spantype=&spanbad;spannewsize=sizes[size];spanregisters=op==1;
        spancall();sx=owned;
        spancheck(allocations==1 && owned!=nil);
        if(op==0)spancheck(spanchild.R.SP==spanchild.R.FP+sizeof(Frame)+32);
        else{
            spancheck(R.SP==sx->reg.tos.fu+sizeof(Frame)+32);
            spancheck(sx->reg.SP==before.SP && sx->reg.TS==before.TS && sx->reg.EX==before.EX);
        }
        releaseextent(owned);spantype=nil;spanregisters=0;checks++;
    }
}


/* Select independent native-error families while retaining every check with NDEBUG. */
int
main(int argc,char **argv)
{
    spancheck(argc==2);
    if(!strcmp(argv[1],"maps"))spanmaps();
    if(!strcmp(argv[1],"geometry"))spangeometry();
    if(!strcmp(argv[1],"capture"))spancapture();
    spancheck(checks>0 && owned==nil);
    printf("PASS stack spans WORD%zu: %d %s cases\n",sizeof(WORD)*8,checks,argv[1]);
    return 0;
}
