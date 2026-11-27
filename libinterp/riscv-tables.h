/* Private RV64 numeric table leaves. RvLayout stays unchanged; the owning
 * runtime supplies its compiled-flag offset explicitly. No RV32 selection. */
enum { Rvrows=11, Rvhalf=12, Rvoffset=13, Rvspan=14 };

static void
rvtableload(RvBuf *b,int result,int pointer,int bytes,int sign)
{
    int i;
    rvi(b,0x03,4,result,pointer,0);
    for(i=1;i<bytes;i++){
        rvi(b,0x03,4,Rvt6,pointer,i);
        rvi(b,0x13,1,Rvt6,Rvt6,8*i);
        rvr(b,0x33,6,0,result,result,Rvt6);
    }
    if(sign && bytes==4){rvi(b,0x13,1,result,result,32);rvi(b,0x13,5,result,result,0x400|32);}
}

/* Conservative complete-span checks; no VM or native stack publication. */
static void
rvtablespan(RvBuf *b,int pointer,int bytes)
{
    rvguard(b,1,pointer,Rvzero);
    rvi(b,0x13,0,Rvt6,Rvzero,-1);rvguard(b,1,pointer,Rvt6);
    if(bytes<=2047)rvi(b,0x13,0,Rvt6,Rvzero,bytes);else rvconstant(b,Rvt6,bytes);
    rvr(b,0x33,0,0,Rvspan,pointer,Rvt6);rvguard(b,6,pointer,Rvspan);
}

static int
rvtableemit(u32int *out,int cap,RvLayout *abi,int op,int compiled)
{
    u32int code[RvMax];RvBuf b;int i,big,header,stride,width,extent,loop,empty,left,right,found;
    if(abi==0 || abi->xlen!=64 || (abi->wordbytes!=4 && abi->wordbytes!=8) || IBY2WD!=4 || cap<=0 ||
       (op!=ICASE && op!=ICASEL && op!=IGOTO) ||
       abi->pc<0 || abi->s<0 || abi->d<0 || abi->pc>2047 || abi->s>2047 || abi->d>2047 ||
       (abi->pc|abi->s|abi->d)%8 || abi->module<0 || abi->module>2047 || abi->module%8 ||
       abi->linkmodule<0 || abi->linkmodule>2047 || abi->linkmodule%8 || compiled<0 || compiled>2047 || compiled%4 ||
       abi->moduleprog<0 || abi->moduleprog>2047 || abi->moduleprog%8 ||
       abi->modulenprog<0 || abi->modulenprog>2047 || abi->modulenprog%4 || abi->instbytes<1 || abi->instbytes>2047)return 0;
    b=(RvBuf){code,0,RvMax,0,abi};big=op==ICASEL;header=big?8:4;stride=big?24:12;width=big?8:abi->wordbytes;
    rvptr(&b,Rvt1,abi->module);
    extent=abi->linkmodule+8>compiled+4?abi->linkmodule+8:compiled+4;
    rvtablespan(&b,Rvt1,extent);
    rvi(&b,0x13,0,Rvt5,Rvt1,compiled);rvtableload(&b,Rvt2,Rvt5,4,1);rvguard(&b,0,Rvt2,Rvzero);
    rvi(&b,0x13,0,Rvt5,Rvt1,abi->linkmodule);rvtableload(&b,Rvt0,Rvt5,8,0);
    extent=abi->moduleprog+8>abi->modulenprog+4?abi->moduleprog+8:abi->modulenprog+4;
    rvtablespan(&b,Rvt0,extent);
    rvptr(&b,Rvt1,abi->s);rvtablespan(&b,Rvt1,width);rvtableload(&b,Rvt3,Rvt1,width,1);
    rvptr(&b,Rvt1,abi->d);
    if(op==IGOTO){
        rvguard(&b,5,Rvt3,Rvzero);rvtablespan(&b,Rvt1,4);
        rvi(&b,0x13,0,Rvt6,Rvzero,4);rvguard(&b,7,Rvt1,Rvt6);
        rvi(&b,0x13,0,Rvt5,Rvt1,-4);rvtablespan(&b,Rvt5,4);rvtableload(&b,Rvt2,Rvt5,4,1);
        rvguard(&b,4,Rvzero,Rvt2);
        rvi(&b,0x13,0,Rvt6,Rvzero,2047);rvi(&b,0x13,1,Rvt6,Rvt6,1);rvi(&b,0x13,6,Rvt6,Rvt6,1);rvguard(&b,7,Rvt6,Rvt2);
        rvguard(&b,6,Rvt3,Rvt2); /* negative signed indices also fail unsigned comparison */
        rvi(&b,0x13,1,Rvoffset,Rvt2,2);rvr(&b,0x33,0,0,Rvspan,Rvt1,Rvoffset);rvguard(&b,6,Rvt1,Rvspan);
        rvi(&b,0x13,1,Rvoffset,Rvt3,2);rvr(&b,0x33,0,0,Rvt1,Rvt1,Rvoffset);
        rvtableload(&b,Rvt4,Rvt1,4,1);
    }else{
        rvtablespan(&b,Rvt1,4);rvtableload(&b,Rvt2,Rvt1,4,1);
        rvi(&b,0x13,0,Rvt6,Rvzero,2047);rvi(&b,0x13,1,Rvt6,Rvt6,1);rvi(&b,0x13,6,Rvt6,Rvt6,1);rvguard(&b,7,Rvt6,Rvt2);
        /* count*12/24 + header + default, using RV64I shifts/adds only. */
        rvi(&b,0x13,1,Rvoffset,Rvt2,1);rvr(&b,0x33,0,0,Rvoffset,Rvoffset,Rvt2);
        rvi(&b,0x13,1,Rvoffset,Rvoffset,big?3:2);rvi(&b,0x13,0,Rvoffset,Rvoffset,header+4);
        rvr(&b,0x33,0,0,Rvt5,Rvt1,Rvoffset);rvguard(&b,6,Rvt1,Rvt5);
        rvi(&b,0x13,0,Rvt5,Rvt5,-4);rvtableload(&b,Rvt4,Rvt5,4,1);
        rvi(&b,0x13,0,Rvrows,Rvt1,header);
        loop=b.n;empty=b.n;rvbranch(&b,0,Rvt2,Rvzero,0);
        rvi(&b,0x13,5,Rvhalf,Rvt2,1);
        rvi(&b,0x13,1,Rvoffset,Rvhalf,1);rvr(&b,0x33,0,0,Rvoffset,Rvoffset,Rvhalf);
        rvi(&b,0x13,1,Rvoffset,Rvoffset,big?3:2);rvr(&b,0x33,0,0,Rvt1,Rvrows,Rvoffset);
        rvtableload(&b,Rvt5,Rvt1,big?8:4,1);
        left=b.n;rvbranch(&b,4,Rvt3,Rvt5,0);
        rvi(&b,0x13,0,Rvoffset,Rvt1,big?8:4);rvtableload(&b,Rvt5,Rvoffset,big?8:4,1);
        right=b.n;rvbranch(&b,5,Rvt3,Rvt5,0);
        rvi(&b,0x13,0,Rvoffset,Rvt1,big?16:8);rvtableload(&b,Rvt4,Rvoffset,4,1);
        found=b.n;rvbranch(&b,0,Rvzero,Rvzero,0);
        rvpatchbranch(&b,left,4,Rvt3,Rvt5);rvi(&b,0x13,0,Rvt2,Rvhalf,0);rvbranch(&b,0,Rvzero,Rvzero,4*(loop-b.n));
        rvpatchbranch(&b,right,5,Rvt3,Rvt5);rvi(&b,0x13,0,Rvrows,Rvt1,stride);
        rvr(&b,0x33,0,32,Rvt2,Rvt2,Rvhalf);rvi(&b,0x13,0,Rvt2,Rvt2,-1);rvbranch(&b,0,Rvzero,Rvzero,4*(loop-b.n));
        rvpatchbranch(&b,empty,0,Rvt2,Rvzero);rvpatchbranch(&b,found,0,Rvzero,Rvzero);
    }
    rvi(&b,0x13,0,Rvt5,Rvt0,abi->modulenprog);rvtableload(&b,Rvt2,Rvt5,4,1);
    rvguard(&b,4,Rvzero,Rvt2);rvguard(&b,6,Rvt4,Rvt2);
    rvi(&b,0x13,0,Rvt5,Rvt0,abi->moduleprog);rvtableload(&b,Rvt0,Rvt5,8,0);
    rvtablespan(&b,Rvt0,abi->instbytes);rvprogramscale(&b,abi->instbytes);
    rvr(&b,0x33,0,0,Rvt4,Rvt0,Rvt4);rvguard(&b,7,Rvt4,Rvt0);
    rvtablespan(&b,Rvt4,abi->instbytes);
    rvs(&b,3,Rva0,Rvt4,abi->pc);rvi(&b,0x13,0,Rva0,Rvzero,1);rvi(&b,0x67,0,Rvzero,Rvra,0);
    if(b.bad || b.n>cap)return 0;
    if(out!=0)for(i=0;i<b.n;i++)out[i]=code[i];
    return b.n;
}
