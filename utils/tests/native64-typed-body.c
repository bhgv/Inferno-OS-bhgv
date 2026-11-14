#include "typed-body-native64.h"

/* Resolve trusted compiler-produced direct slots in the actual mapped owner. */
static void *typedslot(unsigned int mode,long offset,void *fp,void *mp,WORD *immediate)
{
    if(mode==AIMM){*immediate=offset;return immediate;}
    check(offset>=0 && (mode==AFP || mode==AMP));
    return (uchar*)(mode==AFP?fp:mp)+offset;
}

static void typedexecute(const Native64TypedStep *steps,unsigned int count,void *fp,void *mp)
{
    Inst program[256],*target;
    const Native64TypedStep *step;
    WORD source,middle;
    unsigned int pc=0,executed=0,mm;
    REG caller=R;
    check(count<nelem(program));
    while(pc<count){
        check(++executed<10000);step=steps+pc;R.PC=program+pc+1;
        if(step->op==IRET){R=caller;return;}
        R.s=USRC(step->mode)==AXXX?nil:typedslot(USRC(step->mode),step->s,fp,mp,&source);
        if(step->op==IJMP || (step->op>=IBEQB && step->op<=IBGEF)){
            check(step->d>=0 && step->d<count);target=program+step->d;R.d=&target;
        }else R.d=typedslot(UDST(step->mode),step->d,fp,mp,&middle);
        mm=step->mode&ARM;
        R.m=mm==AXNON?R.d:typedslot(mm==AXIMM?AIMM:mm==AXINF?AFP:AMP,step->m,fp,mp,&middle);
        switch(step->op){
        case INOP:break;
        case IMOVW:movw();break;case IMOVP:movp();break;case IMOVL:movl();break;case IMOVF:movf();break;
        case IADDW:addw();break;case ISUBW:subw();break;case IADDL:addl();break;case IADDF:addf();break;
        case ICVTWL:cvtwl();break;case ICVTWF:cvtwf();break;
        case ILENC:lenc();break;case ILENL:lenl();break;
        case IJMP:jmp();break;
        case IBEQW:beqw();break;case IBNEW:bnew();break;case IBLTW:bltw();break;
        case IBLEW:blew();break;case IBGTW:bgtw();break;case IBGEW:bgew();break;
        default:abort();
        }
        check(R.PC>=program && R.PC<=program+count);
        pc=R.PC-program;
        epochs(1);
    }
    check(0);
}

static void typedbodytests(void)
{
    static const int ns[]={-3,0,1,7,100};
    static const char textbytes[]="Gr\303\274\303\237e\0\344\270\226\347\225\214";
    static const LONG bigs[]={-0x20000000000000LL,-17,0,0x20000000000000LL};
    Native64Result owner={H,0}; Type *type;
    String *text,*result; void *fp,*mp; REG saved=R;
    uchar map[SlotBytes/64+1];
    unsigned int i,j,at,total,mpbase,trial;
    WORD n,answer,expected;LONG big,observed;REAL real,realresult;
    struct Params {uchar header[64];void *context;List *argv;String *text;WORD n;};
    check(typed_run_args[0]==offsetof(struct Params,context) && typed_run_args[1]==offsetof(struct Params,argv));
    check(typed_run_args[2]==offsetof(struct Params,text) && typed_run_args[3]==offsetof(struct Params,n));
    for(trial=0;trial<2;trial++) for(i=0;i<nelem(ns);i++){
        contextstart(0,Pready);sourcebankpool=1;memset(map,0,sizeof(map));
        mpbase=typed_run_frame_bytes;total=mpbase+typed_run_data_bytes;check(total<=SlotBytes);
        for(j=0;j<sizeof(typed_run_frame_map);j++)map[j]=typed_run_frame_map[j];
        for(j=0;j<typed_run_data_bytes/8;j++)if(typed_run_data_map[j/8]&(0x80>>(j%8))){at=mpbase/8+j;map[at/8]|=0x80>>(at%8);}
        type=dtype(freeheap,total,map,(total+63)/64);check(type!=nil);
        check(native64resultadopt(&owner,H2D(void*,heapz(type))));fp=owner.value;mp=(uchar*)fp+mpbase;
        n=ns[i];memmove((uchar*)fp+typed_run_args[3],&n,4);
        if(trial){
            text=c2string((char*)textbytes,sizeof(textbytes)-1);R.s=&text;R.d=(uchar*)fp+typed_run_args[2];movp();destroy(text);
            for(j=0;j<2;j++){R.s=&text;R.d=(uchar*)fp+typed_run_args[1];consp();epochs(1);}
        }
        epochs(6);
        typedexecute(typed_run,nelem(typed_run),fp,mp);
        memmove(&answer,(uchar*)mp+typed_run_answer,4);expected=(n>0?n*(n+1)/2:0)+(trial?10:0);check(answer==expected);
        memmove(&result,(uchar*)mp+typed_run_result,sizeof(result));
        memmove(&text,(uchar*)fp+typed_run_args[2],sizeof(text));check(result==text);
        if(trial)check(text!=H && text->len==-8 && D2H(text)->ref==4);else check(text==H);
        native64resultclear(&owner);epochs(6);freetype(type);check(contextlive==0 && sourcebanklive==0 && Tstring.ref==1 && Tlist.ref==1 && Tptr.ref==1);sourcebankpool=0;
    }
    for(i=0;i<nelem(bigs);i++){
        contextstart(0,Pready);sourcebankpool=1;memset(map,0,sizeof(map));
        mpbase=typed_mix_frame_bytes;total=mpbase+typed_mix_data_bytes;check(total<=SlotBytes);
        for(j=0;j<sizeof(typed_mix_frame_map);j++)map[j]=typed_mix_frame_map[j];
        for(j=0;j<typed_mix_data_bytes/8;j++)if(typed_mix_data_map[j/8]&(0x80>>(j%8))){at=mpbase/8+j;map[at/8]|=0x80>>(at%8);}
        type=dtype(freeheap,total,map,(total+63)/64);check(type!=nil);
        check(native64resultadopt(&owner,H2D(void*,heapz(type))));fp=owner.value;mp=(uchar*)fp+mpbase;
        big=bigs[i];real=-1.25;n=-17;
        memmove((uchar*)fp+typed_mix_args[0],&big,8);memmove((uchar*)fp+typed_mix_args[1],&real,8);memmove((uchar*)fp+typed_mix_args[2],&n,4);
        typedexecute(typed_mix,nelem(typed_mix),fp,mp);
        memmove(&observed,(uchar*)mp+typed_mix_outbig,8);memmove(&realresult,(uchar*)mp+typed_mix_outreal,8);
        check(observed==big-17 && realresult==-18.25);
        native64resultclear(&owner);epochs(6);freetype(type);check(contextlive==0 && sourcebanklive==0);sourcebankpool=0;
    }
    for(trial=0;trial<2;trial++)for(n=-1;n<=1;n++){
        contextstart(0,Pready);sourcebankpool=1;memset(map,0,sizeof(map));
        mpbase=typed_guard_frame_bytes;total=mpbase+typed_guard_data_bytes;check(total<=SlotBytes);
        for(j=0;j<sizeof(typed_guard_frame_map);j++)map[j]=typed_guard_frame_map[j];
        for(j=0;j<typed_guard_data_bytes/8;j++)if(typed_guard_data_map[j/8]&(0x80>>(j%8))){at=mpbase/8+j;map[at/8]|=0x80>>(at%8);}
        type=dtype(freeheap,total,map,(total+63)/64);check(type!=nil);
        check(native64resultadopt(&owner,H2D(void*,heapz(type))));fp=owner.value;mp=(uchar*)fp+mpbase;
        memmove((uchar*)fp+typed_guard_args[1],&n,4);
        if(trial){text=c2string((char*)textbytes,sizeof(textbytes)-1);R.s=&text;R.d=(uchar*)fp+typed_guard_args[0];movp();destroy(text);}
        typedexecute(typed_guard,nelem(typed_guard),fp,mp);
        memmove(&answer,(uchar*)mp+typed_guard_answer,4);check(answer==(n<0?-1:trial?8:0));
        memmove(&result,(uchar*)mp+typed_guard_result,sizeof(result));
        memmove(&text,(uchar*)fp+typed_guard_args[0],sizeof(text));check(result==(n<0?H:text));
        if(trial)check(D2H(text)->ref==(n<0?1:2));
        native64resultclear(&owner);epochs(6);freetype(type);check(contextlive==0 && sourcebanklive==0 && Tstring.ref==1);sourcebankpool=0;
    }
    R=saved;process.R=R;
    printf("PASS complete typed bodies: 20 managed-parameter/loop/early-return/nil/Unicode/NUL/big/real executions with actual VM handlers and GC between instructions\n");
}
