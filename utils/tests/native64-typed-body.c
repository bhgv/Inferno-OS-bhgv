#include "typed-body-native64.h"
#include "native64-typed-return.h"

/* Resolve trusted compiler-produced direct slots in the actual mapped owner. */
static void *typedslot(unsigned int mode,long offset,void *fp,void *mp,WORD *immediate)
{
    if(mode==AIMM){*immediate=offset;return immediate;}
    check(offset>=0 && (mode==AFP || mode==AMP));
    return (uchar*)(mode==AFP?fp:mp)+offset;
}

static void typedexecutereturn(const Native64TypedStep *steps,unsigned int count,void *fp,void *mp,Native64TypedReturn *binding)
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
        }else if(step->returned){
            check(UDST(step->mode)==(AFP|AIND) && step->d==32);
            R.d=native64typedreturnslot(binding,step->returned&15,!!(step->returned&16));check(R.d!=nil);
        }else R.d=typedslot(UDST(step->mode),step->d,fp,mp,&middle);
        mm=step->mode&ARM;
        R.m=mm==AXNON?R.d:typedslot(mm==AXIMM?AIMM:mm==AXINF?AFP:AMP,step->m,fp,mp,&middle);
        switch(step->op){
        case INOP:break;
        case IMOVB:movb();break;case IMOVW:movw();break;case IMOVP:movp();break;case IMOVL:movl();break;case IMOVF:movf();break;
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

static void typedexecute(const Native64TypedStep *steps,unsigned int count,void *fp,void *mp)
{
    typedexecutereturn(steps,count,fp,mp,nil);
}

static void typedreturntests(void)
{
    const Native64TypedStep *programs[]={typed_count,typed_identity,typed_wide,typed_fraction,typed_octet,typed_items};
    const unsigned int counts[]={nelem(typed_count),nelem(typed_identity),nelem(typed_wide),nelem(typed_fraction),nelem(typed_octet),nelem(typed_items)};
    const unsigned int frames[]={typed_count_frame_bytes,typed_identity_frame_bytes,typed_wide_frame_bytes,typed_fraction_frame_bytes,typed_octet_frame_bytes,typed_items_frame_bytes};
    const unsigned int datas[]={typed_count_data_bytes,typed_identity_data_bytes,typed_wide_data_bytes,typed_fraction_data_bytes,typed_octet_data_bytes,typed_items_data_bytes};
    const uchar *fmaps[]={typed_count_frame_map,typed_identity_frame_map,typed_wide_frame_map,typed_fraction_frame_map,typed_octet_frame_map,typed_items_frame_map};
    const uchar *dmaps[]={typed_count_data_map,typed_identity_data_map,typed_wide_data_map,typed_fraction_data_map,typed_octet_data_map,typed_items_data_map};
    const unsigned int args[]={typed_count_args[0],typed_identity_args[0],typed_wide_args[0],typed_fraction_args[0],typed_octet_args[0],typed_items_args[0]};
    Native64Result frame={H,0},caller={H,0};Native64TypedReturn binding={0};
    Type *ft,*ct;uchar map[SlotBytes/64+1],cm[]={0,0};
    unsigned int kind,trial,j,at,total,width,ref,probe;int before;
    WORD n,word;UWORD narrow=0xa5c3f00dU,narrowafter;LONG big,observed;REAL real,realout;void *slot,*pointer,*oldpointer;
    String *text,*old;List *items=H,*returned;uvlong canary=0xfeed123456789abcULL,after;
    REG saved=R;
    BYTE bytevalue;uchar byteguard[7];
    for(kind=0;kind<6;kind++)for(trial=0;trial<4;trial++){
        contextstart(0,Pready);sourcebankpool=1;memset(map,0,sizeof(map));
        ref=kind==1 || kind==5;width=kind==0?4:kind==4?1:8;total=frames[kind]+datas[kind];check(total<=SlotBytes);
        for(j=0;j<(frames[kind]+63)/64;j++)map[j]=fmaps[kind][j];
        for(j=0;j<datas[kind]/8;j++)if(dmaps[kind][j/8]&(0x80>>(j%8))){at=frames[kind]/8+j;map[at/8]|=0x80>>(at%8);}
        ft=dtype(freeheap,total,map,(total+63)/64);check(ft!=nil);cm[1]=ref?0x80:0;
        ct=dtype(freeheap,80,cm,2);check(ct!=nil);
        check(native64resultadopt(&frame,H2D(void*,heapz(ft))) && native64resultadopt(&caller,H2D(void*,heapz(ct))));
        slot=(uchar*)caller.value+64;memmove((uchar*)caller.value+72,&canary,8);
        if(kind==0){n=trial==0?-3:trial==1?0:trial==2?7:100;memmove((uchar*)frame.value+args[kind],&n,4);word=-77;memmove(slot,&word,4);memmove((uchar*)slot+4,&narrow,4);}
        else if(kind==1){
            text=trial==0?H:c2string("Gr\303\274\303\237e\0\344\270\226\347\225\214",14);
            R.s=&text;R.d=(uchar*)frame.value+args[kind];movp();if(text!=H)destroy(text);
            old=trial==2?text:c2string("old",3);R.s=&old;R.d=slot;movp();if(trial!=2)destroy(old);
        }else if(kind==2){big=trial==0?(-0x7fffffffffffffffLL-1):trial==1?-17:trial==2?0:0x7fffffffffffffffLL;memmove((uchar*)frame.value+args[kind],&big,8);}
        else if(kind==3){real=trial==0?-1.25:trial==1?0:trial==2?1.0:1.0e100;memmove((uchar*)frame.value+args[kind],&real,8);}
        else if(kind==4){bytevalue=trial==0?0:trial==1?1:trial==2?128:255;memmove((uchar*)frame.value+args[kind],&bytevalue,1);memset((uchar*)slot+1,0xa5,7);}
        else{
            if(trial){
                text=c2string("value",5);R.s=&text;R.d=(uchar*)frame.value+frames[kind]+typed_items_result;movp();destroy(text);
                for(j=0;j<2;j++){R.s=&text;R.d=(uchar*)frame.value+args[kind];consp();epochs(1);}
            }
            memmove(&items,(uchar*)frame.value+args[kind],sizeof(items));
            if(trial==2){R.s=&items;R.d=slot;movp();}
        }
        before=contextattempt;
        for(probe=0;probe<10;probe++){
            unsigned int offset=64,w=width,r=ref;int rooted=frame.rooted;
            if(probe==0)offset=0;if(probe==1)offset=width==1?80:65;if(probe==2)offset=80;if(probe==3)w=2;
            if(probe==4)r=!ref;if(probe==5)r=2;if(probe==6)frame.rooted=0;
            if(probe==7)ft->map[0]=1;if(probe==8)ct->size=63;
            if(probe==9){pointer=H;memmove((uchar*)frame.value+32,&pointer,sizeof(pointer));}
            check(!native64typedreturnbind(&binding,&frame,&caller,offset,w,r));
            frame.rooted=rooted;ft->map[0]=0;ct->size=80;
            pointer=nil;memmove((uchar*)frame.value+32,&pointer,sizeof(pointer));
            check(binding.callee==nil && contextattempt==before);
        }
        /* A valid scalar slot with an insufficient declared extent must not resolve for the opcode. */
        if(!ref && width>1){
            check(native64typedreturnbind(&binding,&frame,&caller,64,1,0));
            check(native64typedreturnslot(&binding,width,0)==nil);
            native64typedreturnclear(&binding);
        }
        check(native64typedreturnbind(&binding,&frame,&caller,64,width,ref));
        check(!native64typedreturnbind(&binding,&frame,&caller,64,width,ref));
        check(native64typedreturnslot(&binding,width,ref)==slot);
        memmove(&oldpointer,(uchar*)frame.value+32,sizeof(oldpointer));
        pointer=(uchar*)caller.value+72;memmove((uchar*)frame.value+32,&pointer,sizeof(pointer));
        check(native64typedreturnslot(&binding,width,ref)==nil);
        memmove((uchar*)frame.value+32,&oldpointer,sizeof(oldpointer));
        caller.rooted=0;check(native64typedreturnslot(&binding,width,ref)==nil);caller.rooted=1;
        ct->map[1]^=0x80;check(native64typedreturnslot(&binding,width,ref)==nil);ct->map[1]^=0x80;
        epochs(6);
        typedexecutereturn(programs[kind],counts[kind],frame.value,(uchar*)frame.value+frames[kind],&binding);
        if(kind==0){memmove(&word,slot,4);memmove(&narrowafter,(uchar*)slot+4,4);check(word==(n>0?n*(n+1)/2:0) && narrowafter==narrow);}
        else if(kind==1){memmove(&old,slot,sizeof(old));check(old==text);if(text!=H)check(text->len==-8 && D2H(text)->ref==2);}
        else if(kind==2){memmove(&observed,slot,8);check(observed==big);}
        else if(kind==3){memmove(&realout,slot,8);check(realout==real);}
        else if(kind==4){check(*(BYTE*)slot==bytevalue);memmove(byteguard,(uchar*)slot+1,7);for(j=0;j<7;j++)check(byteguard[j]==0xa5);}
        else{memmove(&returned,slot,sizeof(returned));check(returned==items);if(items!=H)check(D2H(items)->t==&Tlist && D2H(items)->ref==2);}
        memmove(&after,(uchar*)caller.value+72,8);check(after==canary);
        native64typedreturnclear(&binding);native64typedreturnclear(&binding);
        native64resultclear(&frame);epochs(6);freetype(ft);
        if(kind==1 && text!=H){memmove(&old,slot,sizeof(old));check(old==text && D2H(text)->ref==1);}
        if(kind==5 && items!=H){memmove(&returned,slot,sizeof(returned));check(returned==items && D2H(items)->ref==1);R.s=slot;R.d=&word;lenl();check(word==2);}
        native64resultclear(&caller);epochs(6);freetype(ct);
        check(contextlive==0 && sourcebanklive==0 && Tstring.ref==1);sourcebankpool=0;
    }
    R=saved;process.R=R;
    printf("PASS typed returns: 24 byte/Word/big/real/String/list calls, old/alias/nil replacement, owner retirement, native pointer/width/map guards and untouched canaries\n");
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
    typedreturntests();
    printf("PASS complete typed bodies: 20 managed-parameter/loop/early-return/nil/Unicode/NUL/big/real executions with actual VM handlers and GC between instructions\n");
}
