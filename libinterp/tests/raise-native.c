

/* Keep byte-image, reference and publication checks active in release-style runs. */
static void
raisecheck(int condition)
{
    if(!condition)abort();
}


/* Guard each fallible RAISE invocation without keeping modified automatic objects. */
static void
raisefailure(char *expected)
{
    raised=nil;
    if(setjmp(boundary)==0){raiseentry();abort();}
    raisecheck(raised==expected);
}


/* Reject invalid decoded cells and native prefixes before conversion or publication. */
static unsigned long
raisespans(void)
{
    static uchar cells[32],types[sizeof(Type)+8],stringbefore[sizeof(String)+64],cachebefore[16];

    static struct {
        Heap heap; // Real retained native owner header for tuple payloads.
        void *message; // First exception tuple field consumed by the actual operation.
    } tuple;
    static Type type;
    static REG before;
    String *text;
    void *value,*message;
    char *expected;
    int kind,offset,shift,prior,limit;
    unsigned long checks=0;

    for(kind=0;kind<9;kind++)for(offset=0;offset<128;offset++)
    for(shift=0;shift<8;shift++)for(prior=0;prior<2;prior++){
        if(getenv("RAISE_SPAN_KIND") && kind!=atoi(getenv("RAISE_SPAN_KIND")))continue;
        limit=kind==0 ? sizeof(void*) : kind==1 || kind==6 ? sizeof(Heap) :
            kind==4 ? O(Type,size)+sizeof(int) : kind==8 ? O(String,data) : 7;
        if(offset>limit || ((kind==2 || kind==3 || kind==5 || kind==7) && offset!=0))continue;
        if((kind==2 || kind==7) && shift==0)continue;
        text=textstring(0);memset(&tuple,0,sizeof(tuple));memset(&type,0,sizeof(type));
        type.size=sizeof(void*);tuple.heap.ref=1;tuple.heap.t=&type;tuple.message=text;
        value=&tuple.message;message=text;expected=exHeap;
        memset(&R,0,sizeof(R));R.s=cells+shift;
        if(kind==0)R.s=offset==0 ? nil : (void*)((uintptr)0-(uintptr)offset);
        if(kind==1){value=(void*)(uintptr)offset;if(offset==0)expected=exNilref;}
        if(kind==2)value=(uchar*)&tuple.message+shift;
        if(kind==3)value=(void*)((uintptr)0-sizeof(void*));
        if(kind==4){tuple.heap.t=offset==0 ? nil : (Type*)((uintptr)0-(uintptr)offset);expected=exType;}
        if(kind==5){
            type.size=sizeof(void*)-1;memmove(types+shift,&type,sizeof(type));tuple.heap.t=(Type*)(types+shift);
            expected=exType;
        }
        if(kind==6){message=(void*)(uintptr)offset;if(offset==0)expected=exNilref;}
        if(kind==7)message=(uchar*)text+shift;
        if(kind==8)message=offset==0 ? nil : (void*)((uintptr)0-(uintptr)offset);
        if(kind==8 && offset==0)expected=exNilref;
        /* H is the existing legal empty message, covered by the baseline matrix. */
        if(kind==8 && offset==1){free(text->tmp);free(D2H(text));continue;}
        tuple.message=message;
        if(kind!=0)memmove(R.s,&value,sizeof(value));
        memmove(&before,&R,sizeof(R));memmove(stringbefore,text,sizeof(stringbefore));
        memmove(cachebefore,text->tmp,sizeof(cachebefore));
        current.exval=prior ? (void*)text : H;D2H(text)->ref=1+prior;attempts=allocationfailure=0;
        raisefailure(expected);
        raisecheck(current.exval==(prior ? (void*)text : H) && tuple.heap.ref==1 &&
                   D2H(text)->ref==1+prior && attempts==0);
        raisecheck(!memcmp(&R,&before,sizeof(R)) && !memcmp(text,stringbefore,sizeof(stringbefore)));
        raisecheck(!memcmp(text->tmp,cachebefore,sizeof(cachebefore)));
        free(text->tmp);free(D2H(text));checks++;
    }
    /* Packed readable Type prefixes are valid and must reach real message conversion. */
    if(!getenv("RAISE_SPAN_KIND"))for(shift=0;shift<8;shift++){
        text=textstring(0);memset(&tuple,0,sizeof(tuple));memset(&type,0,sizeof(type));
        type.size=sizeof(void*);memmove(types+shift,&type,sizeof(type));
        tuple.heap.ref=1;tuple.heap.t=(Type*)(types+shift);tuple.message=text;value=&tuple.message;
        memset(&R,0,sizeof(R));R.s=cells+shift;memmove(R.s,&value,sizeof(value));current.exval=H;
        raised=nil;attempts=allocationfailure=0;
        if(setjmp(boundary)==0){raiseentry();abort();}
        raisecheck(current.exval==value && tuple.heap.ref==2 && !strcmp(raised,"er"));
        free(text->tmp);free(D2H(text));checks++;
    }
    raisecheck(checks!=0);
    return checks;
}


/* Publish one actual C-only JIT entry, then run baseline and malformed-prefix matrices. */
int
main(void)
{
    Module module={0};
    Inst instruction={0};
    unsigned long spans=0;
    char *family=getenv("RAISE_NATIVE_CASE");

    instruction.op=IRAISE;module.prog=&instruction;module.nprog=1;optab[IRAISE]=iraise;
    raisecheck(compile(&module,1,nil) && module.jit && module.jit->entry[IRAISE]==nil && module.jit->dispatch[IRAISE]!=nil);
    raisejit=module.jit;
    if(family && !strcmp(family,"baseline"))raisebaseline();
    else spans=raisespans();
    module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
    printf("PASS native RAISE WORD%zu route%s: %lu additional span/packed-Type cases\n",sizeof(WORD)*8,getenv("RAISE_NATIVE_ROUTE"),spans);
    return 0;
}
