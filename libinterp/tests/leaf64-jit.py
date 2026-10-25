#!/usr/bin/env python3
"""Execute actual AMD64, AArch64 and RV64 leaf instructions with independent integer oracles."""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_ARCH_RISCV, CS_MODE_64, CS_MODE_RISCV64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_ARCH_RISCV, UC_MODE_64, UC_MODE_RISCV64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR
from unicorn.riscv_const import UC_RISCV_REG_A0, UC_RISCV_REG_RA

FP_ORACLES = r'''
#include <fenv.h>
/* Compute the rounding/exception contract in host C, restoring the caller environment. */
int realcastoracle(uint64_t bits,int fixed,int mode,int seed,int64_t *out){
    fenv_t saved;
    int flags, result=0, modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    double value;
    volatile double scale=fixed==2 ? -1.0 : 1.0;
    fegetenv(&saved);fesetround(modes[mode]);feclearexcept(FE_ALL_EXCEPT);
    if(seed)feraiseexcept(FE_DIVBYZERO);
    memcpy(&value,&bits,8);
    if(fixed)value*=scale;
    value=value<0 ? value-0.5 : value+0.5;
    *out=(int64_t)value;
    flags=fetestexcept(FE_ALL_EXCEPT);
    if(flags&FE_INVALID)result|=1;
    if(flags&FE_DIVBYZERO)result|=2;
    if(flags&FE_OVERFLOW)result|=4;
    if(flags&FE_UNDERFLOW)result|=8;
    if(flags&FE_INEXACT)result|=16;
    fesetenv(&saved);return result;
}
/* Evaluate the separately rounded fixed product for supported arbitrary power scales. */
int realscaleoracle(uint64_t bits,uint64_t scalebits,int mode,int seed,int64_t *out){
    fenv_t saved;
    int flags,result=0,modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    double value,rawscale;
    volatile double scale;
    memcpy(&value,&bits,8);memcpy(&rawscale,&scalebits,8);scale=rawscale;
    fegetenv(&saved);fesetround(modes[mode]);feclearexcept(FE_ALL_EXCEPT);
    if(seed)feraiseexcept(FE_DIVBYZERO);
    value=value*scale;value=value<0 ? value-0.5 : value+0.5;*out=(int64_t)value;
    flags=fetestexcept(FE_ALL_EXCEPT);
    if(flags&FE_INVALID)result|=1;
    if(flags&FE_DIVBYZERO)result|=2;
    if(flags&FE_OVERFLOW)result|=4;
    if(flags&FE_UNDERFLOW)result|=8;
    if(flags&FE_INEXACT)result|=16;
    fesetenv(&saved);return result;
}
/* Compute the C repeated-squaring sequence without libm pow substitution. */
int realpoweroracle(uint64_t bits,int64_t n,int mode,int seed,uint64_t *out){
    fenv_t saved;
    int flags,status=0,modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    uint64_t exponent=(uint64_t)n;
    double x,result=1.0;
    fegetenv(&saved);fesetround(modes[mode]);feclearexcept(FE_ALL_EXCEPT);
    if(seed)feraiseexcept(FE_DIVBYZERO);
    memcpy(&x,&bits,8);
    if(n<0){exponent=0-exponent;x=1/x;}
    for(;;){if(exponent&1)result*=x;if((exponent>>=1)==0)break;x*=x;}
    memcpy(out,&result,8);flags=fetestexcept(FE_ALL_EXCEPT);
    if(flags&FE_INVALID)status|=1;
    if(flags&FE_DIVBYZERO)status|=2;
    if(flags&FE_OVERFLOW)status|=4;
    if(flags&FE_UNDERFLOW)status|=8;
    if(flags&FE_INEXACT)status|=16;
    fesetenv(&saved);return status;
}
/* Read the standard sticky flags without including platform denormal-input flags. */
static int normalizedflags(void){
    int flags=fetestexcept(FE_ALL_EXCEPT),status=0;
    if(flags&FE_INVALID)status|=1;
    if(flags&FE_DIVBYZERO)status|=2;
    if(flags&FE_OVERFLOW)status|=4;
    if(flags&FE_UNDERFLOW)status|=8;
    if(flags&FE_INEXACT)status|=16;
    return status;
}
/* Convert through volatile native values so the requested rounding mode is observed. */
int integerrealoracle(int64_t input,int mode,uint64_t *out){
    fenv_t saved;
    int flags,modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    volatile int64_t value=input;
    volatile double result;
    double bits;
    fegetenv(&saved);fesetround(modes[mode]);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);
    result=value;bits=result;memcpy(out,&bits,8);flags=normalizedflags();fesetenv(&saved);return flags;
}
/* Preserve raw signaling NaN inputs until the native resizing operation. */
int realresizeoracle(uint64_t input,int width,int mode,uint64_t *out){
    fenv_t saved;
    int flags,modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    float small;double large;
    volatile float result32;
    volatile double result64;
    fegetenv(&saved);fesetround(modes[mode]);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);
    *out=0;
    if(width==4){memcpy(&small,&input,4);result64=small;large=result64;memcpy(out,&large,8);}
    else{memcpy(&large,&input,8);result32=large;small=result32;memcpy(out,&small,4);}
    flags=normalizedflags();fesetenv(&saved);return flags;
}
'''

root = Path(__file__).resolve().parents[2]
ops = '''IMOVMP ISELF ICASE ICASEL IGOTO IBEQC IBNEC IBLTC IBLEC IBGTC IBGEC ITCMP IMOVPC IMULX1 IDIVX1 ICVTXX1 IMULX0 IDIVX0 ICVTXX0 IINDX IMULX IDIVX ICVTRF ICVTFR ICVTWF ICVTLF ICVTXX IBEQF IBNEF IBLTF IBLEF IBGTF IBGEF IHEADM IEXPW IEXPL IINDC ILENL IHEADB IHEADW IHEADL IHEADF ILENC IMOVM INEGF ILENA IINDB IINDW IINDL IINDF IRUNT IMOVB IMOVW IMOVL IMOVF ICVTBW ICVTWB ICVTWS ICVTSW ILEA IJMP
IADDB IADDW ISUBB ISUBW IMULB IMULW IANDB IANDW IORB IORW IXORB IXORW
IADDL ISUBL IMULL IANDL IORL IXORL IBEQB IBEQW IBNEB IBNEW IBLTB IBLTW
IBLEB IBLEW IBGTB IBGTW IBGEB IBGEW ICVTWL ICVTLW ISHLB ISHLW ISHLL ISHRB ISHRW ISHRL ILSRW ILSRL IDIVB IDIVW IDIVL IMODB IMODW IMODL IBEQL IBNEL IBLTL IBLEL IBGTL IBGEL IHEADP IHEADMP ITAIL IMOVP IFRAME ICALL IRET IADDF ISUBF IMULF IDIVF ICVTXF ICVTFW ICVTFL ICVTFX IEXPF'''.split()
class Layout(C.Structure):
    _fields_ = [(x, C.c_int) for x in ('wordbytes', 'pc', 's', 'd', 'm', 'arraylen', 'arraydata', 'stringlen', 'stringdata', 'listdata', 'listtail', 'fp', 'fixedtmp', 'arraytype', 'typesize','framelr','framefp','fixedcoef','module','linkmodule','moduleprog','modulenprog','instbytes','heaptype','linkcompiled','heapref','heapcolor','refbytes')]+[('mutator',C.c_void_p),('nprop',C.c_void_p),('markcolor',C.c_int)]+[(x,C.c_int) for x in ('moduletypes','modulenty','typenp')]
class RvLayout(C.Structure):
    _fields_ = [(x, C.c_int) for x in ('xlen', 'wordbytes', 'hasm', 'pc', 's', 'd', 'm', 'arraylen', 'arraydata', 'stringlen', 'stringdata', 'listdata', 'listtail', 'fp', 'fixedtmp', 'arraytype', 'typesize','hasdiv','framelr','framefp','fixedcoef','module','linkmodule','moduleprog','modulenprog','instbytes','heaptype')]


def signed(value, bits):
    """Interpret a bit vector independently of the emitter's chosen machine instructions."""
    value &= (1 << bits)-1
    return value-(1 << bits) if value >> (bits-1) else value


case=os.environ.get('LEAF64_CASE','')
selected_arch=os.environ.get('LEAF64_ARCH','')
if case not in ('','guard','array','control','copy','string','head','listlen','character','power','headcopy','realbranch','scalar','fixed','integerreal','resize','framefixed','frameabi','descriptor','call','coefficient','roundedcoefficient','movpc','address','typecheck','multiply','bytebranch','realarithmetic','realinteger','realpower','divide','vncoefficient','realbranchfp','goto','case'):raise ValueError(case)
if selected_arch not in ('','amd64','arm64','riscv64'):raise ValueError(selected_arch)
checks = 0
rng = random.Random(0x6c696d626f)
with tempfile.TemporaryDirectory(prefix='leaf64-jit-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64', 'riscv64'):
        if selected_arch and arch!=selected_arch:continue
        rv = arch == 'riscv64'
        source = td/(arch+'.c')
        source.write_text('#include <stdint.h>\n#include <string.h>\n'
                          'typedef uint32_t u32int; typedef unsigned char uchar;\n#define nil 0\n'
                          '#include "isa.h"\n#include "'+(os.environ.get('LEAF64_RISCV_EMITTER','riscv-emit.h') if rv else os.environ.get('LEAF64_'+arch.upper()+'_EMITTER',arch+'-emit.h'))+'"\n'
                          'int emit(u32int *p,int n,'+('RvLayout' if rv else 'LeafLayout')+' *l,int op){return '+('rvemit' if rv else 'leafemit')+'(p,n,l,op);}\n'
                          'int opid(int i){static int ops[]={'+','.join(ops)+'};return ops[i];}\n')
        if case in ('','realinteger','realpower','resize','integerreal'):
            with source.open('a') as output:
                output.write(FP_ORACLES)
        binary = td/(arch+'.so')
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd+['-shared','-fPIC','-I'+str(root/'include'),'-I'+str(root/'libinterp'),str(source),'-lm','-o',str(binary)],check=True)
        lib = C.CDLL(str(binary))
        ids = {name:lib.opid(i) for i,name in enumerate(ops)}
        for wb in (4,8):
            layout = RvLayout(64,wb,1,0,8,16,24) if rv else Layout(wb,0,8,16,24)
            if rv:
                profile=os.environ.get('RV_FEATURE_PROFILE','m')
                if profile not in ('i','zmmul','m'):raise ValueError(profile)
                layout.hasm=int(profile!='i');layout.hasdiv=int(profile=='m')
            if not rv:
                layout.linkcompiled=16;layout.heapref=24;layout.heapcolor=32;layout.refbytes=8;layout.mutator=0x100002000;layout.nprop=0x100002008;layout.markcolor=3;layout.moduletypes=32;layout.modulenty=40;layout.typenp=28
            layout.heaptype=24;layout.framelr=0;layout.framefp=8
            layout.arraylen=8;layout.arraydata=32;layout.stringlen=12;layout.stringdata=32;layout.listdata=24;layout.listtail=8;layout.fp=32;layout.fixedtmp=28;layout.fixedcoef=20;layout.module=40;layout.linkmodule=8;layout.moduleprog=16;layout.modulenprog=24;layout.instbytes=24;layout.arraytype=16;layout.typesize=24
            dis = Cs(CS_ARCH_X86,CS_MODE_64) if arch == 'amd64' else Cs(CS_ARCH_ARM64,0) if arch == 'arm64' else Cs(CS_ARCH_RISCV,CS_MODE_RISCV64)
            blobs = {}
            for name in ops:
                out = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                n = lib.emit(out,64,C.byref(layout),ids[name])
                blobs[name] = bytes(out)[:n*4]
                assert lib.emit(None,64,C.byref(layout),ids[name]) == n
                if n:
                    assert sum(i.size for i in dis.disasm(blobs[name],0x10000)) == n*4
                    guard = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,n-1,C.byref(layout),ids[name]) == 0
                    assert list(guard)[n-1:] == [0xa5a5a5a5]*(65-n)
                checks += 1
            # Reject malformed frame descriptions before exposing any emitted output.
            for name in ('IMULX','IDIVX'):
                invalid=[('fp',-1),('fp',1),('fp',2048),('fixedtmp',-1),('fixedtmp',2048)]
                if rv:invalid.append(('fixedtmp',1))
                for field,value in invalid:
                    malformed=type(layout).from_buffer_copy(layout)
                    setattr(malformed,field,value)
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids[name])==0
                    assert list(guard)==[0xa5a5a5a5]*64
                    checks+=1
            for field in ('arraylen','arraydata','arraytype','typesize'):
                for value in (-1,1,2048):
                    malformed=type(layout).from_buffer_copy(layout)
                    setattr(malformed,field,value)
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids['IINDX'])==0
                    assert list(guard)==[0xa5a5a5a5]*64
                    checks+=1
            for field in ('fp','framelr','framefp'):
                for value in (-1,1,2048):
                    malformed=type(layout).from_buffer_copy(layout);setattr(malformed,field,value)
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids['ICALL'])==0
                    assert list(guard)==[0xa5a5a5a5]*64
                    checks+=1
            malformed=type(layout).from_buffer_copy(layout);malformed.framefp=malformed.framelr
            assert lib.emit(None,64,C.byref(malformed),ids['ICALL'])==0
            checks+=1
            for name in ('IMULX0','IDIVX0','ICVTXX0','IMULX1','IDIVX1','ICVTXX1'):
                for field,value in [('fixedcoef',-1),('fixedcoef',2048)]+([('fixedcoef',1)] if rv else []):
                    malformed=type(layout).from_buffer_copy(layout);setattr(malformed,field,value)
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids[name])==0
                    assert list(guard)==[0xa5a5a5a5]*64;checks+=1
            for field in ('module','linkmodule','moduleprog','modulenprog','instbytes'):
                for value in (-1,1 if field!='instbytes' else 0,2048):
                    malformed=type(layout).from_buffer_copy(layout);setattr(malformed,field,value)
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids['IMOVPC'])==0
                    assert list(guard)==[0xa5a5a5a5]*64;checks+=1
            if case=='frameabi':continue
            for name in (('ISELF','IHEADP','ITAIL','IMOVP') if rv else ())+(('IMOVMP','IHEADMP') if rv else ())+('IFRAME','IRET'):
                assert not blobs[name]
            code, registers, data, stack = 0x10000, 0x100020000, 0x100030000, 0x40000
            oldpc, target = 0x123456789, 0x987654321

            if case in ('','case'):
                if rv:
                    assert not blobs['ICASE'] and not blobs['ICASEL']
                else:
                    for name in ('ICASE','ICASEL'):
                        assert blobs[name],(arch,wb,name,'missing numeric case leaf')
                        for field,value in [('linkcompiled',-1),('linkcompiled',1),('moduleprog',1),('modulenprog',1),('instbytes',0)]:
                            malformed=type(layout).from_buffer_copy(layout);setattr(malformed,field,value)
                            guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                            assert lib.emit(guard,64,C.byref(malformed),ids[name])==0 and list(guard)==[0xa5a5a5a5]*64
                            checks+=1

                    def casecheck(name,n,value,pattern=0,seed=0,sa=0,ta=0,alias=0,bad=None):
                        """Compare hardware search with a linear oracle over fixed-width rows."""
                        if arch=='amd64':u=Uc(UC_ARCH_X86,UC_MODE_64);arg,result=UC_X86_REG_RDI,UC_X86_REG_RAX
                        else:u=Uc(UC_ARCH_ARM64,0);arg=result=UC_ARM64_REG_X0
                        for address in (code,registers,stack):u.mem_map(address,0x1000)
                        u.mem_map(data,0x20000);u.mem_write(code,blobs[name])
                        image=bytearray(b'\xa5'*0x20000)
                        wide=name=='ICASEL';width=8 if wide else wb;header=8 if wide else 4;stride=24 if wide else 12
                        table=data+0x2000+ta;source=data+0x100+sa
                        base=-128 if pattern==0 else 0 if pattern==1 else 1<<40 if wide else -(1<<31)
                        defaults=(-1,0,31,32);destination=defaults[seed]
                        struct.pack_into('<i',image,table-data,n)
                        rows=[]
                        for row in range(max(n,0)):
                            low=base+row*16;high=low+8;entry=(row*7+seed)%35-1
                            at=table-data+header+row*stride
                            struct.pack_into('<qqi' if wide else '<iii',image,at,low,high,entry)
                            rows.append((low,high,entry))
                        struct.pack_into('<i',image,table-data+header+max(n,0)*stride,destination)
                        if alias:source=table if alias==1 else table+header
                        else:image[source-data:source-data+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')
                        actual=signed(int.from_bytes(image[source-data:source-data+width],'little'),width*8)
                        for low,high,entry in rows:
                            if low<=actual<high:destination=entry;break
                        valid=0<=n<=4095 and 0<=destination<32
                        link=data+0x700;owner=data+0x900;program=data+0xc00;length=32;compiled=0
                        if bad==0:link=0
                        if bad==1:link=(1<<64)-1
                        if bad==2:owner=0
                        if bad==3:owner=(1<<64)-1
                        if bad==4:compiled=1;owner=1
                        if bad==5:length=-1
                        if bad==6:length=0
                        if bad==7:program=0
                        if bad==8:program=(1<<64)-1
                        if bad==9:program=(1<<64)-9
                        if bad==10:table=0
                        if bad==11:table=(1<<64)-1
                        if bad==12:table=(1<<64)-2
                        if bad is not None:
                            valid=False
                            if bad<=4:source=data+0x30000
                        struct.pack_into('<Q',image,0x700+layout.linkmodule,owner)
                        struct.pack_into('<i',image,0x700+layout.linkcompiled,compiled)
                        struct.pack_into('<Q',image,0x900+layout.moduleprog,program)
                        struct.pack_into('<i',image,0x900+layout.modulenprog,length)
                        u.mem_write(data,bytes(image))
                        regs=bytearray(b'\xa5'*0x1000);regs[:40]=struct.pack('<QQQQQ',oldpc,source,table,data+0x30000,data+0x40000)
                        struct.pack_into('<Q',regs,layout.module,link)
                        expected=bytearray(regs)
                        if valid:struct.pack_into('<Q',expected,0,program+destination*layout.instbytes)
                        u.mem_write(registers,bytes(regs));u.reg_write(arg,registers)
                        end=code+0x800
                        if arch=='amd64':u.reg_write(UC_X86_REG_RSP,stack+0x800);u.mem_write(stack+0x800,struct.pack('<Q',end))
                        else:u.reg_write(UC_ARM64_REG_LR,end)
                        if n<0 or n>4095:
                            def only_header(uc,access,address,size,value,user):
                                assert not data+0x2000+ta+4<=address<data+0x1f000,(arch,name,n,'unexpected table payload read')
                            u.hook_add(UC_HOOK_MEM_READ,only_header)
                        u.emu_start(code,end,count=2000)
                        assert u.reg_read(result)==int(valid),(arch,wb,name,n,value,pattern,seed,bad)
                        assert bytes(u.mem_read(registers,0x1000))==bytes(expected),(arch,wb,name,'PC/REG',n,value,pattern,seed,bad)
                        assert bytes(u.mem_read(data,0x20000))==bytes(image)

                    for name in ('ICASE','ICASEL'):
                        for n in (-1,0,1,2,7,8):
                            for value in (-129,-128,-121,-120,-1,0,1,7,8,15,16,127,1<<40,-(1<<63),(1<<63)-1):
                                for seed in range(4):
                                    for sa,ta in ((0,0),(1,7),(4,1),(7,4)):
                                        casecheck(name,n,value,seed=seed,sa=sa,ta=ta);checks+=1
                        for pattern in (1,2):
                            for value in (0,1,7,8,15,16,-(1<<31),(1<<31)-1,1<<40,(1<<40)+7,(1<<40)+8):
                                for alias in range(3):
                                    casecheck(name,7,value,pattern=pattern,seed=1,sa=1,ta=3,alias=alias);checks+=1
                        for n in (33,4095,4096):
                            for value in (-1,0,7,8,16,(n-1)*16,(n-1)*16+7,(n-1)*16+8):
                                casecheck(name,n,value,pattern=1,seed=2,sa=1,ta=1);checks+=1
                        for bad in range(13):casecheck(name,0,0,seed=2,bad=bad);checks+=1
                if case=='case':continue
            if case in ('','goto'):
                if rv:
                    assert not blobs['IGOTO']
                else:
                    assert blobs['IGOTO'],(arch,wb,'missing native goto')
                    for field,value in [('linkcompiled',-1),('linkcompiled',1),('linkcompiled',2048)]:
                        malformed=type(layout).from_buffer_copy(layout);setattr(malformed,field,value)
                        guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                        assert lib.emit(guard,64,C.byref(malformed),ids['IGOTO'])==0 and list(guard)==[0xa5a5a5a5]*64
                        checks+=1

                    def gotocheck(index,count,destination,sa=0,ta=0,alias=0,bad=None):
                        """Check exact REG/table images using an independent packed-table decoder."""
                        if arch=='amd64':u=Uc(UC_ARCH_X86,UC_MODE_64);arg,result=UC_X86_REG_RDI,UC_X86_REG_RAX
                        else:u=Uc(UC_ARCH_ARM64,0);arg=result=UC_ARM64_REG_X0
                        for address in (code,registers,data,stack):u.mem_map(address,0x1000)
                        u.mem_write(code,blobs['IGOTO'])
                        image=bytearray(b'\xa5'*0x1000)
                        table=data+0x300+ta;source=data+0x100+sa
                        if alias:source=table-(4 if alias==1 else 0)
                        image[table-data-4:table-data]=struct.pack('<i',count)
                        for row in range(16):image[table-data+row*4:table-data+row*4+4]=struct.pack('<I',(destination+row%3)&0xffffffff)
                        if not alias:image[source-data:source-data+wb]=(index&((1<<(8*wb))-1)).to_bytes(wb,'little')
                        actualindex=signed(int.from_bytes(image[source-data:source-data+wb],'little'),wb*8)
                        valid=count>0 and 0<=actualindex<count
                        if valid:
                            entry=struct.unpack_from('<i',image,table-data+actualindex*4)[0]
                            valid=0<=entry<32
                        link=data+0x700;owner=data+0x900;program=data+0xc00;length=32;compiled=0
                        if bad==0:link=0
                        if bad==1:link=(1<<64)-1
                        if bad==2:owner=0
                        if bad==3:owner=(1<<64)-1
                        if bad==4:compiled=1;owner=1
                        if bad==5:length=-1
                        if bad==6:length=0
                        if bad==7:program=0
                        if bad==8:program=(1<<64)-1
                        if bad==9:program=(1<<64)-9
                        if bad==10:table=0
                        if bad==11:table=(1<<64)-1
                        if bad is not None:
                            valid=False
                            if bad<=4:source=data+0x2000
                        image[0x700+layout.linkmodule:0x700+layout.linkmodule+8]=struct.pack('<Q',owner)
                        image[0x700+layout.linkcompiled:0x700+layout.linkcompiled+4]=struct.pack('<i',compiled)
                        image[0x900+layout.moduleprog:0x900+layout.moduleprog+8]=struct.pack('<Q',program)
                        image[0x900+layout.modulenprog:0x900+layout.modulenprog+4]=struct.pack('<i',length)
                        u.mem_write(data,bytes(image))
                        regs=bytearray(b'\xa5'*0x1000)
                        regs[:40]=struct.pack('<QQQQQ',oldpc,source,table,data+0x2000,data+0x3000)
                        regs[layout.module:layout.module+8]=struct.pack('<Q',link)
                        expected=bytearray(regs)
                        if valid:expected[:8]=struct.pack('<Q',program+entry*layout.instbytes)
                        u.mem_write(registers,bytes(regs));u.reg_write(arg,registers)
                        end=code+0x800
                        if arch=='amd64':
                            u.reg_write(UC_X86_REG_RSP,stack+0x800);u.mem_write(stack+0x800,struct.pack('<Q',end))
                        else:u.reg_write(UC_ARM64_REG_LR,end)
                        u.emu_start(code,end,count=1000)
                        assert u.reg_read(result)==int(valid),(arch,wb,'IGOTO',index,count,destination,bad)
                        assert bytes(u.mem_read(registers,0x1000))==bytes(expected),(arch,wb,'goto PC/REG',index,count,destination,bad)
                        assert bytes(u.mem_read(data,0x1000))==bytes(image)

                    for index in (-1,0,1,7,15,16,-(1<<(8*wb-1)),(1<<(8*wb-1))-1,1<<32):
                        for count in (-1,0,1,2,8,16):
                            for destination in (-1,0,1,31,32,-(1<<31),(1<<31)-1):
                                for sa in (0,1,4,7):
                                    for ta in (0,1,4,7):
                                        for alias in range(3):
                                            gotocheck(index,count,destination,sa,ta,alias);checks+=1
                    for bad in range(12):gotocheck(0,2,1,bad=bad);checks+=1
                if case=='goto':continue
            if case in ('','call'):
                # Check complete REG/data images, including source/target/frame aliases.
                for sa in ((0,4) if rv else range(8)):
                    for da in ((0,4) if rv else range(8)):
                        for framealign in ((0,4) if rv else range(8)):
                            for alias in range(5):
                                if arch=='amd64':u=Uc(UC_ARCH_X86,UC_MODE_64);arg,result=UC_X86_REG_RDI,UC_X86_REG_RAX
                                elif arch=='arm64':u=Uc(UC_ARCH_ARM64,0);arg=result=UC_ARM64_REG_X0
                                else:u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64);arg=result=UC_RISCV_REG_A0
                                for address in (code,registers,data,stack):u.mem_map(address,0x1000)
                                u.mem_write(code,blobs['ICALL'])
                                frame=data+0x500+framealign;previous=data+0x900
                                sp=data+0x100+sa;dp=data+0x200+da
                                if alias in (1,2):dp=frame+(layout.framelr if alias==1 else layout.framefp)
                                if alias in (3,4):sp=frame+(layout.framelr if alias==3 else layout.framefp)
                                image=bytearray(b'\xa5'*0x1000)
                                image[sp-data:sp-data+8]=struct.pack('<Q',frame)
                                image[dp-data:dp-data+8]=struct.pack('<Q',target)
                                u.mem_write(data,bytes(image))
                                expected=bytearray(image)
                                expected[frame-data+layout.framelr:frame-data+layout.framelr+8]=struct.pack('<Q',oldpc)
                                expected[frame-data+layout.framefp:frame-data+layout.framefp+8]=struct.pack('<Q',previous)
                                u.mem_write(registers,struct.pack('<QQQQQ',oldpc,sp,dp,0xdeadbeef,previous))
                                u.reg_write(arg,registers);end=code+0x800
                                if arch=='amd64':u.reg_write(UC_X86_REG_RSP,stack+0x800);u.mem_write(stack+0x800,struct.pack('<Q',end))
                                else:u.reg_write(UC_ARM64_REG_LR if arch=='arm64' else UC_RISCV_REG_RA,end)
                                if rv:
                                    def aligned_call(uc,access,address,size,value,user):
                                        assert address%size==0,(hex(address),size)
                                    u.hook_add(UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,aligned_call)
                                u.emu_start(code,end,count=1000)
                                assert u.reg_read(result)==1,(arch,wb,alias)
                                assert bytes(u.mem_read(data,0x1000))==expected,(arch,wb,alias,framealign)
                                assert bytes(u.mem_read(registers,40))==struct.pack('<QQQQQ',target,sp,dp,0xdeadbeef,frame)
                                checks+=1
                # Rejected frames must not read even an unmapped target slot.
                for frame in ((0,(1<<64)-1,data+0x501,data+0x502,data+0x503) if rv else (0,(1<<64)-1)):
                    if arch=='amd64':u=Uc(UC_ARCH_X86,UC_MODE_64);arg,result=UC_X86_REG_RDI,UC_X86_REG_RAX
                    elif arch=='arm64':u=Uc(UC_ARCH_ARM64,0);arg=result=UC_ARM64_REG_X0
                    else:u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64);arg=result=UC_RISCV_REG_A0
                    for address in (code,registers,data,stack):u.mem_map(address,0x1000)
                    u.mem_write(code,blobs['ICALL']);u.mem_write(data+0x104,struct.pack('<Q',frame))
                    saved=struct.pack('<QQQQQ',oldpc,data+0x104,0x100060000,0xdeadbeef,data+0x900)
                    u.mem_write(registers,saved);u.reg_write(arg,registers);end=code+0x800
                    if arch=='amd64':u.reg_write(UC_X86_REG_RSP,stack+0x800);u.mem_write(stack+0x800,struct.pack('<Q',end))
                    else:u.reg_write(UC_ARM64_REG_LR if arch=='arm64' else UC_RISCV_REG_RA,end)
                    u.emu_start(code,end,count=1000)
                    assert u.reg_read(result)==0 and bytes(u.mem_read(registers,40))==saved
                    checks+=1
                if case=='call':continue

            def run(name, a=0, b=0, alias=0, raw=None, expect=1, poison=False, array=None, arrayalias=0, copy=None, string=None, head=None, chain=None, character=None, headcopy=None, badtarget=False, badsource=False, fixedframe=None, descriptor=None, badresult=False, programaddress=None, slotshift=0, typecheck=None, stringpair=None, fpstate=None, branchstate=None):
                """Exercise a leaf with high native pointers, four-byte alignment and exact aliases."""
                if arch == 'amd64':
                    u = Uc(UC_ARCH_X86,UC_MODE_64); arg, result = UC_X86_REG_RDI, UC_X86_REG_RAX
                elif arch == 'arm64':
                    u = Uc(UC_ARCH_ARM64,0); arg = result = UC_ARM64_REG_X0
                else:
                    u = Uc(UC_ARCH_RISCV,UC_MODE_RISCV64); arg = result = UC_RISCV_REG_A0
                for address in (code,registers,data,stack): u.mem_map(address,0x1000)
                u.mem_write(code,blobs[name])
                s,m,d = data+0x104+slotshift,data+0x204+slotshift,data+0x304+slotshift
                if fixedframe is not None and fixedframe[2] is not None:d=fixedframe[0]+layout.fixedtmp+fixedframe[2]
                if alias == 1: d=s
                if alias == 2: d=m
                if alias == 3: s=d-4
                if alias == 4: d=m+4
                if alias == 5: s=d+4
                output=d
                metadata=None
                typedata=None
                expected_copy=None
                expected_head=None
                if stringpair is not None:
                    left,leftlen,leftdata,right,rightlen,rightdata=stringpair
                    u.mem_write(s,struct.pack('<Q',left));u.mem_write(m,struct.pack('<Q',right))
                    for address,length,payload in ((data+0x500,leftlen,leftdata),(data+0x700,rightlen,rightdata)):
                        header=bytearray(b'\x5a'*256)
                        header[layout.stringlen:layout.stringlen+4]=struct.pack('<i',length)
                        header[layout.stringdata:layout.stringdata+len(payload)]=payload
                        u.mem_write(address,bytes(header))
                    u.mem_write(d,struct.pack('<Q',target))
                    expected_copy=bytes(u.mem_read(data,0x1000))
                elif typecheck is not None:
                    sourceowner,destowner,sourcetype,desttype=typecheck
                    u.mem_write(s,struct.pack('<Q',sourceowner))
                    u.mem_write(d,struct.pack('<Q',destowner))
                    u.mem_write(data+0x540-layout.heaptype,struct.pack('<Q',sourcetype))
                    u.mem_write(data+0x740-layout.heaptype,struct.pack('<Q',desttype))
                    expected_copy=bytes(u.mem_read(data,0x1000))
                elif headcopy is not None:
                    pointer,length,delta,countslot=headcopy
                    s=data+0x104;m=data+countslot;d=data+0x500+layout.listdata+delta
                    if alias==1:d=s
                    if alias==2:d=m
                    output=d
                    memory=bytearray((i*29+17)&255 for i in range(0x1000))
                    memory[0x104:0x10c]=struct.pack('<Q',pointer)
                    memory[countslot:countslot+wb]=(length&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    u.mem_write(data,bytes(memory))
                    if length>0 and expect:
                        memory[d-data:d-data+length]=bytes(memory[0x500+layout.listdata:0x500+layout.listdata+length])
                    expected_copy=bytes(memory)
                    if poison:
                        if pointer in (0,(1<<64)-1):m=0x100050000
                        d=0x100050000;output=data+0x100
                elif character is not None:
                    owner,length,index,values=character
                    if poison:owner=data+0x1000-layout.stringdata
                    if alias==1:d=s
                    if alias==2:d=m
                    if alias==3:d=data+0x500+layout.stringdata+4
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    metadata=bytearray(b'\x5a'*128)
                    metadata[layout.stringlen:layout.stringlen+4]=struct.pack('<i',length)
                    for i,bits in enumerate(values):
                        pos=layout.stringdata+i*(4 if length<0 else 1)
                        width=4 if length<0 else 1
                        metadata[pos:pos+width]=(bits&((1<<(width*8))-1)).to_bytes(width,'little')
                    u.mem_write(owner if poison else data+0x500,bytes(metadata[:layout.stringdata] if poison else metadata))
                    u.mem_write(s,struct.pack('<Q',owner))
                    u.mem_write(m,(index&((1<<(8*wb))-1)).to_bytes(wb,'little'))
                    expected_head=bytearray(u.mem_read(data,0x1000))
                    if expect:
                        value=signed(values[index],32) if length<0 else values[index]&255
                        expected_head[d-data:d-data+wb]=(value&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    metadata=None
                elif chain is not None:
                    length,terminal=chain
                    if alias:d=s
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    first=data+0x500 if length else terminal
                    u.mem_write(s,struct.pack('<Q',first))
                    for i in range(length):
                        node=data+0x500+i*16
                        tail=data+0x500+(i+1)*16 if i+1<length else terminal
                        u.mem_write(node+layout.listtail,struct.pack('<Q',tail))
                    expected_head=bytearray(u.mem_read(data,0x1000))
                    if expect:expected_head[d-data:d-data+wb]=length.to_bytes(wb,'little')
                elif head is not None:
                    owner,bits=head
                    if alias==1:d=s
                    if alias==2:d=data+0x500+layout.listdata-4
                    if alias==3:d=data+0x500+layout.listdata+4
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    metadata=bytearray(b'\x5a'*64)
                    metadata[layout.listdata:layout.listdata+8]=struct.pack('<Q',bits)
                    u.mem_write(data+0x500,bytes(metadata))
                    u.mem_write(s,struct.pack('<Q',owner))
                    expected_head=bytearray(u.mem_read(data,0x1000))
                    if expect:
                        width=1 if name=='IHEADB' else wb if name=='IHEADW' else 8
                        expected_head[d-data:d-data+width]=expected_head[0x500+layout.listdata:0x500+layout.listdata+width]
                elif string is not None:
                    owner,length=string
                    if alias:d=s
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    u.mem_write(s,struct.pack('<Q',owner))
                    metadata=bytearray(b'\x5a'*64)
                    metadata[layout.stringlen:layout.stringlen+4]=struct.pack('<i',length)
                    u.mem_write(data+0x500,bytes(metadata))
                elif copy is not None:
                    src,dst,length,countslot=copy
                    s=data+src;d=data+dst;m=data+countslot;output=d
                    memory=bytearray((i*29+17)&255 for i in range(0x1000))
                    memory[countslot:countslot+wb]=(length&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    u.mem_write(data,bytes(memory))
                    if length>0 and expect:memory[dst:dst+length]=bytes(memory[src:src+length])
                    expected_copy=bytes(memory)
                    if length<=0 or not expect:s=d=0x100050000;output=data+0x100
                elif array is None:
                    u.mem_write(d-8,b'\xa5'*32)
                    u.mem_write(s,raw if raw is not None else struct.pack('<Q',a & ((1<<64)-1)))
                    if not poison: u.mem_write(m,struct.pack('<Q',b & ((1<<64)-1)))
                    else: m=0x100050000
                    if name.startswith('IB') or name == 'IJMP': u.mem_write(d,struct.pack('<Q',target))
                else:
                    owner,length,index,backing=array
                    if name=='ILENA':
                        if arrayalias:d=s
                        output=d
                    else:
                        if arrayalias==1:m=d
                        if arrayalias==2:m=s
                        output=m
                    u.mem_write(output-8,b'\xa5'*32)
                    u.mem_write(m,struct.pack('<Q',123))
                    u.mem_write(s,struct.pack('<Q',owner))
                    if name!='ILENA':u.mem_write(d,struct.pack('<Q',index&((1<<64)-1)))
                    metadata=bytearray(b'\x5a'*64)
                    metadata[layout.arraylen:layout.arraylen+wb]=(length&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    metadata[layout.arraydata:layout.arraydata+8]=struct.pack('<Q',backing)
                    if descriptor is not None:
                        typeptr,width=descriptor
                        metadata[layout.arraytype:layout.arraytype+8]=struct.pack('<Q',typeptr)
                        typedata=bytearray(b'\x6b'*64)
                        typedata[layout.typesize:layout.typesize+4]=struct.pack('<i',width)
                        u.mem_write(data+0x700,bytes(typedata))
                    u.mem_write(data+0x500,bytes(metadata))
                if programaddress is not None:
                    link,owner,backing,length=programaddress
                    linkbytes=bytearray(b'\x7b'*64);ownerbytes=bytearray(b'\x6b'*64)
                    linkbytes[layout.linkmodule:layout.linkmodule+8]=struct.pack('<Q',owner)
                    ownerbytes[layout.moduleprog:layout.moduleprog+8]=struct.pack('<Q',backing)
                    ownerbytes[layout.modulenprog:layout.modulenprog+4]=struct.pack('<i',length)
                    u.mem_write(data+0x500,bytes(linkbytes));u.mem_write(data+0x700,bytes(ownerbytes))
                fp=data+0x900 if fixedframe is None else fixedframe[0]
                if fixedframe is not None and fp not in (0,(1<<64)-1):
                    u.mem_write(fp+layout.fixedtmp,(fixedframe[1]&((1<<(8*wb))-1)).to_bytes(wb,'little'))
                    if len(fixedframe)>3:u.mem_write(fp+layout.fixedcoef,(fixedframe[3]&((1<<(8*wb))-1)).to_bytes(wb,'little'))
                before = bytes(u.mem_read(output-8,32))
                if badresult:m=0x100060000
                if badtarget:d=0x100060000
                if badsource:s=0x100060000
                u.mem_write(registers,struct.pack('<QQQQQ',oldpc,s,d,m,fp))
                if programaddress is not None:u.mem_write(registers+layout.module,struct.pack('<Q',programaddress[0]))
                u.reg_write(arg,registers)
                end=code+0x800
                if arch == 'amd64':
                    u.reg_write(UC_X86_REG_RSP,stack+0x800); u.mem_write(stack+0x800,struct.pack('<Q',end))
                else: u.reg_write(UC_ARM64_REG_LR if arch == 'arm64' else UC_RISCV_REG_RA,end)
                if rv:
                    def aligned_access(uc, access, address, size, value, user):
                        assert address % size == 0, (name, hex(address), size)
                    u.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, aligned_access)
                if stringpair is not None:
                    def exact_string_reads(uc,access,address,size,value,user):
                        for base,length in ((data+0x500,stringpair[1]),(data+0x700,stringpair[4])):
                            start=base+layout.stringdata
                            if start<=address<base+256:
                                limit=start+abs(length)*(4 if length<0 else 1)
                                assert address+size<=limit,(arch,name,hex(address),size,length)
                    u.hook_add(UC_HOOK_MEM_READ,exact_string_reads)
                if fpstate is not None:
                    mode,seed=fpstate
                    if arch=='amd64':u.reg_write(UC_X86_REG_MXCSR,0x1f80 | mode<<13 | (4 if seed else 0))
                    else:
                        u.reg_write(UC_ARM64_REG_FPCR,(0,2,1,3)[mode]<<22)
                        u.reg_write(UC_ARM64_REG_FPSR,2 if seed else 0)
                if branchstate is not None:
                    mode,flush,seed=branchstate
                    control=(0x1f80 | mode<<13 | (flush&1)<<6 | (flush>>1)<<15) if arch=='amd64' else ((0,2,1,3)[mode]<<22 | flush<<24)
                    u.reg_write(UC_X86_REG_MXCSR if arch=='amd64' else UC_ARM64_REG_FPCR,control | (4 if seed and arch=='amd64' else 0))
                    if arch=='arm64':u.reg_write(UC_ARM64_REG_FPSR,2 if seed else 0)
                u.emu_start(code,end,count=4000)
                if expected_head is not None:assert bytes(u.mem_read(data,0x1000))==bytes(expected_head),(arch,wb,name,head,alias)
                if expected_copy is not None:assert bytes(u.mem_read(data,0x1000))==expected_copy,(arch,wb,copy)
                assert u.reg_read(result) == expect,(arch,wb,name,a,b,u.reg_read(result),expect)
                assert bytes(u.mem_read(registers+8,32)) == struct.pack('<QQQQ',s,d,m,fp)
                if programaddress is not None:
                    assert bytes(u.mem_read(data+0x500,64))==linkbytes and bytes(u.mem_read(data+0x700,64))==ownerbytes
                    assert int.from_bytes(u.mem_read(registers+layout.module,8),'little')==programaddress[0]
                if typedata is not None:assert bytes(u.mem_read(data+0x700,64))==bytes(typedata)
                if metadata is not None and head is None:assert bytes(u.mem_read(data+0x500,64))==metadata
                if branchstate is not None:
                    hardware=u.reg_read(UC_X86_REG_MXCSR if arch=='amd64' else UC_ARM64_REG_FPSR)
                    if arch=='amd64':assert hardware&~63==control
                    else:assert u.reg_read(UC_ARM64_REG_FPCR)==control
                    return int.from_bytes(u.mem_read(registers,8),'little'),before,bytes(u.mem_read(output-8,32)),hardware& (63 if arch=='amd64' else 255)
                if fpstate is not None:
                    if arch=='amd64':
                        status=u.reg_read(UC_X86_REG_MXCSR)
                        assert status&~63==0x1f80 | fpstate[0]<<13
                        status=(status&1) | ((status>>1)&30)
                    else:
                        assert u.reg_read(UC_ARM64_REG_FPCR)==(0,2,1,3)[fpstate[0]]<<22
                        status=u.reg_read(UC_ARM64_REG_FPSR)&31
                    return bytes(u.mem_read(output,16)),before,bytes(u.mem_read(output-8,32)),status
                return bytes(u.mem_read(output,16)), int.from_bytes(u.mem_read(registers,8),'little'), before, bytes(u.mem_read(output-8,32)),s

            if case in ('','realpower'):
                if rv:assert not blobs['IEXPF']
                else:
                    lib.realpoweroracle.argtypes=[C.c_uint64,C.c_int64,C.c_int,C.c_int,C.POINTER(C.c_uint64)]
                    values=(0,0x8000000000000000,1,0x8000000000000001,
                            0x3ff0000000000000,0xbff0000000000000,0x4000000000000000,0xc000000000000000,
                            0x3ff0000000000001,0x3fe0000000000000,0x0010000000000000,0x7fefffffffffffff,
                            0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000042,0x7ff0000000000042)
                    exponents=(0,1,-1,2,-2,3,-3,31,32,63,64,127,-127,-(1<<(wb*8-1)),(1<<(wb*8-1))-1)
                    for bits in values:
                        for exponent in exponents:
                            for mode in range(4):
                                wanted=C.c_uint64()
                                flags=lib.realpoweroracle(bits,exponent,mode,1,C.byref(wanted))
                                for alias in (0,1,2,3,4,5):
                                    out,before,after,status=run('IEXPF',a=exponent,b=bits,alias=alias,fpstate=(mode,1))
                                    actual=int.from_bytes(out[:8],'little')
                                    if wanted.value&0x7fffffffffffffff>0x7ff0000000000000:assert actual&0x7fffffffffffffff>0x7ff0000000000000
                                    else:assert actual==wanted.value,(arch,wb,hex(bits),exponent,mode,hex(actual),hex(wanted.value))
                                    if arch=='arm64':assert status==flags,(arch,wb,hex(bits),exponent,mode,status,flags)
                                    assert after[:8]==before[:8] and after[16:]==before[16:];checks+=1
                if case=='realpower':continue

            if case in ('','realinteger'):
                names=('ICVTFW','ICVTFL','ICVTFX')
                if rv:assert not any(blobs[name] for name in names)
                else:
                    patterns=[0,1,0x8000000000000000,0x8000000000000001,
                              0x3fe0000000000000,0xbfe0000000000000,
                              0x41cfffffffffffff,0x41d0000000000000,0xc1d0000000000000,
                              0x43cfffffffffffff,0x43d0000000000000,0xc3d0000000000000,
                              0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000042,0x7ff0000000000042]
                    patterns += [bound+delta | sign for bound in (0x41dfffffffe00000,0x43dfffffffffffff)
                                 for delta in (-2,-1,0,1,2) for sign in (0,1<<63)]
                    patterns += [rng.getrandbits(64) for _ in range(200)]
                    def fixedvalid(bits,scale,width):
                        """Classify supported exact products by exponent ranges and an independent bit value."""
                        magnitude=scale&0x7fffffffffffffff
                        exponent=(magnitude>>52)-1023
                        if magnitude&((1<<52)-1) or not -16<=exponent<=16:return False
                        source=(bits>>52)&2047
                        if exponent and (not 1<=source<=2046 or not 1<=source+exponent<=2046):return False
                        product=(bits&0x7fffffffffffffff)+exponent*(1<<52)
                        return product<(0x41dfffffffe00000 if width==4 else 0x43dfffffffffffff)

                    scales=(0x3ff0000000000000,0xbff0000000000000,0,0x4000000000000000,0x7ff0000000000042,
                            0xc000000000000000,0x3fe0000000000000,0xbfe0000000000000,
                            0x40f0000000000000,0xc0f0000000000000,0x3ef0000000000000,0xbef0000000000000,
                            0x4100000000000000,0x3ff8000000000000)
                    for name in names:
                        width=8 if name=='ICVTFL' else wb
                        for bits in patterns:
                            value=struct.unpack('<d',struct.pack('<Q',bits))[0]
                            valid=(bits&0x7fffffffffffffff)<(0x41dfffffffe00000 if width==4 else 0x43dfffffffffffff)
                            for scale in (scales if name=='ICVTFX' else (0,)):
                                take=fixedvalid(bits,scale,width) if name=='ICVTFX' else valid
                                for alias in range(6):
                                    out,pc,before,after,_=run(name,a=bits,b=scale,alias=alias,expect=int(take))
                                    if take:
                                        scaled=value*struct.unpack('<d',struct.pack('<Q',scale))[0] if name=='ICVTFX' else value
                                        wanted=int(scaled-0.5 if scaled<0 else scaled+0.5)
                                        assert signed(int.from_bytes(out[:width],'little'),width*8)==wanted,(arch,name,bits)
                                        assert after[:8]==before[:8] and after[8+width:]==before[8+width:]
                                    else:assert before==after
                                    assert pc==oldpc;checks+=1
                    lib.realcastoracle.argtypes=[C.c_uint64,C.c_int,C.c_int,C.c_int,C.POINTER(C.c_int64)]
                    for name in names:
                        width=8 if name=='ICVTFL' else wb
                        for bits in patterns[:36]:
                            valid=(bits&0x7fffffffffffffff)<(0x41dfffffffe00000 if width==4 else 0x43dfffffffffffff)
                            for mode in range(4):
                                for seed in range(2):
                                    for negative in range(2 if name=='ICVTFX' else 1):
                                        wanted=C.c_int64()
                                        flags=lib.realcastoracle(bits,(1+negative) if name=='ICVTFX' else 0,mode,seed,C.byref(wanted)) if valid else (2 if seed else 0)
                                        for alias in (0,1,4):
                                            out,before,after,status=run(name,a=bits,b=0xbff0000000000000 if negative else 0x3ff0000000000000,alias=alias,expect=int(valid),fpstate=(mode,seed))
                                            # Unicorn x86 omits conversion precision flags; native tests check those.
                                            if not valid or arch=='arm64':assert status==flags,(arch,wb,name,hex(bits),mode,seed,status,flags)
                                            if valid:assert signed(int.from_bytes(out[:width],'little'),width*8)==wanted.value
                                            else:assert before==after
                                            checks+=1
                    lib.realscaleoracle.argtypes=[C.c_uint64,C.c_uint64,C.c_int,C.c_int,C.POINTER(C.c_int64)]
                    boundary=patterns[:36]+[0x0010000000000000,0x001fffffffffffff,0x7fefffffffffffff,0x800fffffffffffff]
                    for bits in boundary:
                        for scale in (0x4000000000000000,0xc000000000000000,0x40f0000000000000,0xc0f0000000000000,0x3ef0000000000000,0xbef0000000000000):
                            take=fixedvalid(bits,scale,wb)
                            for mode in range(4):
                                for seed in range(2):
                                    wanted=C.c_int64()
                                    flags=lib.realscaleoracle(bits,scale,mode,seed,C.byref(wanted)) if take else (2 if seed else 0)
                                    for alias in (0,4):
                                        out,before,after,status=run('ICVTFX',a=bits,b=scale,alias=alias,expect=int(take),fpstate=(mode,seed))
                                        if not take or arch=='arm64':assert status==flags,(arch,wb,'scaled flags',hex(bits),hex(scale),mode,status,flags)
                                        if take:assert signed(int.from_bytes(out[:wb],'little'),wb*8)==wanted.value
                                        else:assert before==after
                                        checks+=1
                if case=='realinteger':continue

            if case in ('','realarithmetic'):
                names=('IADDF','ISUBF','IMULF','IDIVF','ICVTXF')
                if rv:
                    assert not any(blobs[name] for name in names)
                else:
                    # Independent host binary64 oracle; NaN payload selection is host-defined.
                    patterns=(0,1,0x8000000000000000,0x8000000000000001,
                              0x0010000000000000,0x3ff0000000000000,0xbff0000000000000,
                              0x3ff0000000000001,0x7fefffffffffffff,0xffefffffffffffff,
                              0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000042)
                    for name in names:
                        for a in patterns:
                            for b in patterns:
                                left=struct.unpack('<d',struct.pack('<Q',b))[0]
                                right=float(signed(a,wb*8)) if name=='ICVTXF' else struct.unpack('<d',struct.pack('<Q',a))[0]
                                if name=='IADDF':wanted=left+right
                                elif name=='ISUBF':wanted=left-right
                                elif name in ('IMULF','ICVTXF'):wanted=left*right
                                elif right==0:
                                    wanted=float('nan') if left==0 or left!=left else float('-inf') if ((b^a)>>63) else float('inf')
                                else:wanted=left/right
                                for alias in range(6):
                                    out,pc,before,after,_=run(name,a=a,b=b,alias=alias)
                                    actual=struct.unpack('<d',out[:8])[0]
                                    if wanted!=wanted:assert actual!=actual,(arch,name,a,b)
                                    else:assert out[:8]==struct.pack('<d',wanted),(arch,name,a,b,out[:8].hex(),wanted)
                                    assert after[:8]==before[:8] and after[16:]==before[16:]
                                    assert pc==oldpc
                                    checks+=1
                if case=='realarithmetic':continue

            if case in ('','bytebranch'):
                names=('IBEQC','IBNEC','IBLTC','IBLEC','IBGTC','IBGEC')
                if rv:
                    assert not any(blobs[name] for name in names)
                else:
                    for name in names:
                        assert blobs[name],(arch,wb,name)
                        for field,value in [('stringlen',-1),('stringlen',1),('stringlen',2048),('stringdata',-1),('stringdata',2048)]:
                            malformed=type(layout).from_buffer_copy(layout);setattr(malformed,field,value)
                            guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                            assert lib.emit(guard,64,C.byref(malformed),ids[name])==0 and list(guard)==[0xa5a5a5a5]*64
                            checks+=1
                    texts=[b'',b'a',b'ab',b'a\0b',b'a\0c',b'\0',b'\x7f',b'\x80',b'\xff',b'x'*128,b'x'*127+b'y']
                    pairs=[(a,b) for a in texts for b in texts]
                    pairs += [(bytes(rng.getrandbits(8) for _ in range(rng.randrange(32))),bytes(rng.getrandbits(8) for _ in range(rng.randrange(32)))) for _ in range(32)]
                    for a,b in pairs:
                        for condition,name in enumerate(names):
                            take=(a==b,a!=b,a<b,a<=b,a>b,a>=b)[condition]
                            for shift in (0,1,4,7):
                                _,pc,before,after,_=run(name,slotshift=shift,badtarget=not take,stringpair=(data+0x500,len(a),a,data+0x700,len(b),b))
                                assert pc==(target if take else oldpc) and before==after
                                checks+=1
                    points=[(),(0,),(127,),(128,),(255,),(256,),(0x10000,),(0x10ffff,),(0x7fffffff,),(0x80000000,),(0xffffffff,),(97,0,98),(97,256),(97,257)]
                    forms=[]
                    for points_ in points:
                        for wide in (False,True):
                            if not wide and any(p>255 for p in points_):continue
                            payload=b''.join(struct.pack('<I',p) for p in points_) if wide else bytes(points_)
                            forms.append((-len(points_) if wide else len(points_),payload,tuple(signed(p,32) for p in points_)))
                    for ll,ld,aa in forms:
                        for rl,rd,bb in forms:
                            for condition,name in enumerate(names):
                                take=(aa==bb,aa!=bb,aa<bb,aa<=bb,aa>bb,aa>=bb)[condition]
                                _,pc,before,after,_=run(name,badtarget=not take,stringpair=(data+0x500,ll,ld,data+0x700,rl,rd))
                                assert pc==(target if take else oldpc) and before==after,(arch,wb,name,ll,rl,aa,bb)
                                checks+=1
                    # Identical owners compare equal without reading even a huge payload.
                    for length in (0,1,-1,0x7fffffff,-0x7fffffff):
                        for condition,name in enumerate(names):
                            take=condition in (0,3,5)
                            for shift in (0,4):
                                _,pc,before,after,_=run(name,slotshift=shift,badtarget=not take,stringpair=(data+0x500,length,b'a',data+0x500,0,b''))
                                assert pc==(target if take else oldpc) and before==after
                                checks+=1
                    for left in (0,(1<<64)-1,data+0x500):
                        for right in (0,(1<<64)-1,data+0x700):
                            for ll in (-0x80000000,-1,0,1):
                                for rl in (-0x80000000,-1,0,1):
                                    valid=left!=0 and right!=0 and (left==(1<<64)-1 or ll!=-0x80000000) and (right==(1<<64)-1 or rl!=-0x80000000)
                                    aa=b'' if left==(1<<64)-1 or ll==0 else b'a'
                                    bb=b'' if right==(1<<64)-1 or rl==0 else b'b'
                                    for condition,name in enumerate(names):
                                        take=(aa==bb,aa!=bb,aa<bb,aa<=bb,aa>bb,aa>=bb)[condition]
                                        _,pc,before,after,_=run(name,expect=int(valid),badtarget=not valid or not take,stringpair=(left,ll,struct.pack('<I',97) if ll<0 else b'a',right,rl,struct.pack('<I',98) if rl<0 else b'b'))
                                        assert pc==(target if valid and take else oldpc) and before==after
                                        checks+=1
                if case=='bytebranch':continue
            if case in ('','multiply'):
                values=[0,1,-1,2,-2,127,128,255,256,0x7fffffff,-0x80000000,(1<<63)-1,-(1<<63)]
                pairs=[(a,b) for a in values for b in values]
                pairs += [(signed(rng.getrandbits(64),64),signed(rng.getrandbits(64),64)) for _ in range(64)]
                for name,width in (('IMULB',1),('IMULW',wb),('IMULL',8)):
                    mask=(1<<(width*8))-1
                    if rv and not layout.hasm:
                        assert not any(i.mnemonic.startswith(('mul','div','rem')) for i in dis.disasm(blobs[name],code))
                    for a,b in pairs:
                        for alias in (0,1,2,3,4,5):
                            out,pc,before,after,_=run(name,a,b,alias=alias)
                            # Both operands are captured before any aliased result store.
                            aa=a&((1<<64)-1);bb=b&((1<<64)-1)
                            wanted=((aa&mask)*(bb&mask))&mask
                            assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,a,b,alias,out,wanted)
                            assert pc==oldpc and before[:8]==after[:8] and before[8+width:]==after[8+width:]
                            checks+=1
                if case=='multiply':continue
            if case in ('','typecheck'):
                for value in (-1,0,1,7,256,2048):
                    malformed=type(layout).from_buffer_copy(layout);malformed.heaptype=value
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids['ITCMP'])==0
                    assert list(guard)==[0xa5a5a5a5]*64
                    checks+=1
                owners=(0,(1<<64)-1,data+0x540,data+0x740)
                types=(0,(1<<64)-1,data+0x900,data+0xa00)
                for source in owners:
                    for destination in owners:
                        for stype in types:
                            for dtype in types:
                                for alias in (0,1):
                                    effective=source if alias else destination
                                    headers={data+0x540:stype,data+0x740:dtype}
                                    valid=source==(1<<64)-1 or (source in headers and effective in headers and headers[source]==headers[effective])
                                    for shift in ((0,4) if rv else range(8)):
                                        _,pc,before,after,_=run('ITCMP',alias=alias,slotshift=shift,expect=int(valid),typecheck=(source,effective,stype,dtype))
                                        assert pc==oldpc and before==after
                                        checks+=1
                # A nil source is accepted without following the destination's header.
                for source in (0,(1<<64)-1):
                    _,pc,before,after,_=run('ITCMP',expect=int(source!=0),typecheck=(source,0x100070000,0,0))
                    assert pc==oldpc and before==after;checks+=1
                if case=='typecheck':continue
            if case in ('','address'):
                for shift in ((0,4) if rv else range(8)):
                    for alias in (0,1,2,3,5):
                        for unreadable in (False,True):
                            out,pc,before,after,source=run('ILEA',alias=alias,badsource=unreadable,slotshift=shift)
                            assert int.from_bytes(out[:wb],'little')==source&((1<<(wb*8))-1)
                            assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            assert pc==oldpc
                            checks+=1
                if case=='address':continue
            if case in ('','movpc'):
                for link in (0,(1<<64)-1,data+0x500):
                    for owner in (0,(1<<64)-1,data+0x700):
                        for length in (-1,0,1,4,0x7fffffff):
                            for index in [-1,0,1,3,0x7fffffff]+([1<<32] if wb==8 else []):
                                for backing in (0,(1<<64)-1,data+0x900,(1<<64)-16):
                                    valid=link==data+0x500 and owner==data+0x700 and 0<=index<length and backing not in (0,(1<<64)-1) and backing+index*layout.instbytes<(1<<64)
                                    for alias in (0,1,3):
                                        out,pc,before,after,_=run('IMOVPC',index,alias=alias,expect=int(valid),programaddress=(link,owner,backing,length),badtarget=not valid)
                                        assert pc==oldpc
                                        if valid:
                                            assert int.from_bytes(out[:8],'little')==backing+index*layout.instbytes
                                            assert before[:8]==after[:8] and before[16:]==after[16:]
                                        else:assert before==after
                                        checks+=1
                for link,owner in ((0,data+0x700),((1<<64)-1,data+0x700),(data+0x500,0),(data+0x500,(1<<64)-1)):
                    _,pc,before,after,_=run('IMOVPC',expect=0,programaddress=(link,owner,data+0x900,4),badsource=True,badtarget=True)
                    assert pc==oldpc and before==after;checks+=1
                if rv:
                    # Every accepted stride must fit the bounded leaf buffer and
                    # execute without MUL/DIV/REM on the base-I profile.
                    for stride in range(1,2048):
                        layout.instbytes=stride
                        out=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                        n=lib.emit(out,64,C.byref(layout),ids['IMOVPC'])
                        assert 0<n<=64,(wb,stride,n)
                        assert lib.emit(None,64,C.byref(layout),ids['IMOVPC'])==n
                        assert list(out)[n:]==[0xa5a5a5a5]*(64-n)
                        guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                        assert lib.emit(guard,n-1,C.byref(layout),ids['IMOVPC'])==0
                        assert list(guard)[n-1:]==[0xa5a5a5a5]*(65-n)
                        blobs['IMOVPC']=bytes(out)[:4*n]
                        decoded=list(dis.disasm(blobs['IMOVPC'],code))
                        assert sum(i.size for i in decoded)==4*n
                        if not layout.hasm:
                            assert not any(i.mnemonic.startswith(('mul','div','rem')) for i in decoded)
                        for index,backing in ((0,data+0x900),(3,data+0x900),(0x7ffffffe,data+0x900),(3,(1<<64)-16)):
                            valid=backing+index*stride<(1<<64)
                            result,pc,before,after,_=run('IMOVPC',index,expect=int(valid),programaddress=(data+0x500,data+0x700,backing,0x7fffffff),badtarget=not valid)
                            assert pc==oldpc
                            if valid:assert int.from_bytes(result[:8],'little')==backing+index*stride
                            else:assert before==after
                            checks+=1
                    layout.instbytes=24
                    out=(C.c_uint32*64)();n=lib.emit(out,64,C.byref(layout),ids['IMOVPC'])
                    blobs['IMOVPC']=bytes(out)[:4*n]
                if case=='movpc':continue
            if case in ('','coefficient'):
                pairs=[(0,0),(1,1),(-1,1),(1,-1),(-7,3),(7,-3),(-(1<<(wb*8-1)),2),((1<<(wb*8-1))-1,-2)]
                pairs += [(signed(rng.getrandbits(wb*8),wb*8),signed(rng.getrandbits(wb*8),wb*8)) for _ in range(4)]
                for name in ('IMULX0','IDIVX0','ICVTXX0'):
                    assert blobs[name],(arch,wb,name,'missing coefficient leaf')
                    for x,y in pairs:
                        for coefficient in (0,1,-1,2,-3):
                            for count in (-64,-63,-1,0,1,31,63,64,-(1<<(wb*8-1))):
                                divisor=y if name=='IDIVX0' else coefficient
                                valid=-63<=count<=63 and divisor!=0 and (not rv or divisor!=-1)
                                numerator=coefficient*x if name=='IDIVX0' else x*y if name=='IMULX0' else x
                                numerator=signed(numerator,64)
                                if valid:
                                    numerator=signed(numerator<<count,64) if count>=0 else numerator>>-count
                                    quotient=abs(numerator)//abs(divisor)
                                    wanted=(-quotient if (numerator<0)!=(divisor<0) else quotient)&((1<<(8*wb))-1)
                                for alias in (0,1,2):
                                    out,pc,before,after,_=run(name,x if name=='ICVTXX0' else y,count if name=='ICVTXX0' else x,alias=alias,expect=int(valid),fixedframe=(data+0x900,count,None,coefficient),badtarget=not valid)
                                    assert pc==oldpc
                                    if valid:assert int.from_bytes(out[:wb],'little')==wanted,(arch,wb,name,x,y,count,coefficient,out,wanted)
                                    else:assert before==after
                                    checks+=1
                    for delta in (0,layout.fixedcoef-layout.fixedtmp,4 if rv else 1):
                        out,pc,before,after,_=run(name,3,-2 if name!='ICVTXX0' else -1,expect=1,fixedframe=(data+0x900,-1,delta,2))
                        value=3 if name=='ICVTXX0' else 3*-2 if name=='IMULX0' else 2*-2
                        value>>=1;divisor=3 if name=='IDIVX0' else 2
                        wanted=-(abs(value)//divisor) if value<0 else value//divisor
                        assert int.from_bytes(out[:wb],'little')==wanted&((1<<(8*wb))-1)
                        assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                        checks+=1
                    for frame in (0,(1<<64)-1):
                        _,pc,before,after,_=run(name,expect=0,badsource=True,poison=True,badtarget=True,fixedframe=(frame,0,None,2))
                        assert pc==oldpc and before==after;checks+=1
                if case=='coefficient':continue
            if case in ('','roundedcoefficient','vncoefficient'):
                pairs=[(0,0),(1,1),(-1,1),(1,-1),(-7,3),(7,-3),(-(1<<(wb*8-1)),2),((1<<(wb*8-1))-1,-2)]
                pairs += [(signed(rng.getrandbits(wb*8),wb*8),signed(rng.getrandbits(wb*8),wb*8)) for _ in range(4)]
                for name in ('IMULX1','IDIVX1','ICVTXX1'):
                    assert blobs[name],(arch,wb,name,'missing coefficient leaf')
                    for x,y in pairs:
                        coefficients=(0,1,-1,2,-3,-(1<<(wb*8-1)),(1<<(wb*8-1))-1,-(1<<(wb*8-1))+1) if case=='vncoefficient' else (0,1,-1,2,-3)
                        counts=[(scale*4+flags,flags) for scale in (-63,-32,-1,0,31,63) for flags in (2,3)] if case=='vncoefficient' else [(scale*4+flags,flags) for scale in (-64,-63,-33,-32,-1,0,1,31,63,64) for flags in range(4)]+[(-(1<<(wb*8-1)),0),((1<<(wb*8-1))-1,3)]
                        for coefficient in coefficients:
                            for count,flags in counts:
                                divisor=y if name=='IDIVX1' else coefficient
                                scale=count>>2
                                zero=x==0 or name=='IMULX1' and y==0
                                opposite=(x<0)!=(y<0) if name!='ICVTXX1' else x<0
                                native_mask=x<=0 if name=='IDIVX1' else opposite ^ (bool(flags&2) and not zero)
                                valid=(-63<=scale<=63 and (flags==0 if rv else not(flags&1) or scale<0 or not native_mask)) and divisor!=0 and (not rv or divisor!=-1)
                                numerator=coefficient*x if name=='IDIVX1' else x*y if name=='IMULX1' else x
                                numerator=signed(numerator,64)
                                if valid:
                                    zero=x==0 or name=='IMULX1' and y==0
                                    correction=0
                                    opposite=(x<0)!=(y<0) if name!='ICVTXX1' else x<0
                                    if flags&2 and not zero:
                                        correction=1 if name=='IDIVX1' else signed(coefficient-1,wb*8)
                                        if opposite:correction=signed(-correction,wb*8)
                                    maskneeded=x<=0 if name=='IDIVX1' else (not opposite if flags&2 else opposite)
                                    if flags&1 and maskneeded and not zero:numerator=signed(numerator+((1<<-scale)-1),64)
                                    numerator=signed(numerator<<scale,64) if scale>=0 else numerator>>-scale
                                    if name!='IDIVX1':numerator=signed(numerator+correction,64)
                                    quotient=abs(numerator)//abs(divisor)
                                    wanted=-quotient if (numerator<0)!=(divisor<0) else quotient
                                    if name=='IDIVX1':wanted+=correction
                                    wanted=(0 if zero else wanted)&((1<<(8*wb))-1)
                                for alias in (0,1,2):
                                    out,pc,before,after,_=run(name,x if name=='ICVTXX1' else y,count if name=='ICVTXX1' else x,alias=alias,expect=int(valid),fixedframe=(data+0x900,count,None,coefficient),badtarget=not valid)
                                    assert pc==oldpc
                                    if valid:assert int.from_bytes(out[:wb],'little')==wanted,(arch,wb,name,x,y,count,coefficient,out,wanted)
                                    else:assert before==after
                                    checks+=1
                    for delta in (0,layout.fixedcoef-layout.fixedtmp,4 if rv else 1):
                        out,pc,before,after,_=run(name,3,-2 if name!='ICVTXX1' else -4,expect=1,fixedframe=(data+0x900,-4,delta,2))
                        value=3 if name=='ICVTXX1' else 3*-2 if name=='IMULX1' else 2*-2
                        value>>=1;divisor=3 if name=='IDIVX1' else 2
                        wanted=-(abs(value)//divisor) if value<0 else value//divisor
                        assert int.from_bytes(out[:wb],'little')==wanted&((1<<(8*wb))-1)
                        assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                        checks+=1
                    for frame in (0,(1<<64)-1):
                        _,pc,before,after,_=run(name,expect=0,badsource=True,poison=True,badtarget=True,fixedframe=(frame,0,None,2))
                        assert pc==oldpc and before==after;checks+=1
                if case in ('roundedcoefficient','vncoefficient'):continue
            if case in ('','descriptor'):
                assert blobs['IINDX'],(arch,wb,'missing descriptor leaf')
                for width in (-1,0,1,3,7,8,16,0x7fffffff):
                    for length in (-1,0,1,8,0x80000001,(1<<63)-1):
                        for index in (-1,0,1,7,length-1,length):
                            for backing in (data+0x800,0,(1<<64)-1,(1<<64)-16):
                                actualindex=signed(index,wb*8);actuallength=signed(length,wb*8)
                                valid=0<=actualindex<actuallength and width>=0 and (width==0 or
                                      backing not in (0,(1<<64)-1) and actualindex*width<=0x7fffffff and backing+actualindex*width<(1<<64))
                                for alias in (0,1,2):
                                    out,pc,before,after,_=run('IINDX',array=(data+0x500,length,index,backing),arrayalias=alias,
                                                            descriptor=(data+0x700,width),expect=int(valid))
                                    if valid:
                                        assert int.from_bytes(out[:wb],'little')==(backing+actualindex*width)&((1<<(8*wb))-1),(arch,width,length,index,backing)
                                        assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                                    else:assert before==after
                                    assert pc==oldpc
                                    checks+=1
                                if not valid:
                                    _,pc,before,after,_=run('IINDX',array=(data+0x500,length,index,backing),
                                                            descriptor=(data+0x700,width),expect=0,badresult=True)
                                    assert pc==oldpc and before==after
                                    checks+=1
                for owner in (0,(1<<64)-1):
                    _,pc,before,after,_=run('IINDX',array=(owner,8,0,data+0x800),descriptor=(data+0x700,3),expect=0,badtarget=True,badresult=True)
                    assert pc==oldpc and before==after
                    checks+=1
                for typeptr in (0,(1<<64)-1):
                    _,pc,before,after,_=run('IINDX',array=(data+0x500,8,3,data+0x800),descriptor=(typeptr,3),expect=0,badresult=True)
                    assert pc==oldpc and before==after
                    checks+=1
                if case=='descriptor':continue
            if case in ('','framefixed'):
                values=[0,1,-1,127,-128,0x7fffffff,-0x80000000,(1<<63)-1,-(1<<63)]
                pairs=[(a,b) for a in values for b in values]+[(signed(rng.getrandbits(64),64),signed(rng.getrandbits(64),64)) for _ in range(16)]
                counts=[-63,-33,-32,-1,0,1,31,32,62,63,-64,64,-(1<<(wb*8-1))]
                for name in ('IMULX','IDIVX'):
                    if not blobs[name]:
                        assert rv and not layout.hasdiv and name=='IDIVX'
                        continue
                    if rv and not layout.hasm:
                        assert not any(i.mnemonic.startswith(('mul','div','rem')) for i in dis.disasm(blobs[name],code))
                    for a,b in pairs:
                        aa,bb=signed(a,wb*8),signed(b,wb*8)
                        for count in counts:
                            valid=-63<=count<=63 and (name=='IMULX' or aa!=0 and (not rv or aa!=-1))
                            for alias in (0,1,2,3,5):
                                out,pc,before,after,_=run(name,a,b,alias=alias,expect=int(valid),fixedframe=(data+0x900,count,None))
                                if valid:
                                    intermediate=signed(aa*bb,64) if name=='IMULX' else bb
                                    intermediate=signed(intermediate<<count,64) if count>=0 else intermediate>>-count
                                    if name=='IDIVX':
                                        magnitude=abs(intermediate)//abs(aa)
                                        intermediate=-magnitude if (intermediate<0)!=(aa<0) else magnitude
                                    assert int.from_bytes(out[:wb],'little')==intermediate&((1<<(8*wb))-1),(arch,wb,name,aa,bb,count,alias)
                                    assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                                else:assert before==after
                                assert pc==oldpc
                                checks+=1
                    for count in range(-63,64):
                        a,b=3,-7
                        for delta in (-4,0,4):
                            out,_,before,after,_=run(name,a,b,fixedframe=(data+0x900,count,delta))
                            value=a*b if name=='IMULX' else b
                            value=signed(value<<count,64) if count>=0 else value>>-count
                            if name=='IDIVX':value=-(abs(value)//a) if value<0 else value//a
                            assert int.from_bytes(out[:wb],'little')==value&((1<<(8*wb))-1)
                            assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            checks+=1
                    if rv:
                        for delta in (1,2,3):
                            _,pc,before,after,_=run(name,expect=0,poison=True,badsource=True,fixedframe=(data+0x900+delta,0,None))
                            assert pc==oldpc and before==after
                            checks+=1
                    for frame in (0,(1<<64)-1):
                        _,pc,before,after,_=run(name,expect=0,poison=True,badsource=True,fixedframe=(frame,0,None))
                        assert pc==oldpc and before==after
                        checks+=1
                    for count in (-64,64,-(1<<(wb*8-1))):
                        _,pc,before,after,_=run(name,expect=0,poison=True,badsource=True,fixedframe=(data+0x900,count,None))
                        assert pc==oldpc and before==after
                        checks+=1
                for divisor in (((0,-1) if rv else (0,)) if blobs['IDIVX'] else ()):
                    _,pc,before,after,_=run('IDIVX',divisor,7,expect=0,poison=True,fixedframe=(data+0x900,0,None))
                    assert pc==oldpc and before==after
                    checks+=1
                if case=='framefixed':continue
            if case in ('','resize'):
                singles=[0,1<<31,1,(1<<31)|1,0x7fffff,0x800000,0x807fffff,0x80800000,
                         0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,
                         0x7fc00042,0x7f800042,0xffc00042,0xff800042]
                singles += [rng.getrandbits(32) for _ in range(128)]
                doubles=[0,1<<63,1,(1<<63)|1,0x10000000000000,0x7fefffffffffffff,
                         0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000042,0x7ff0000000000042]
                for bits in singles:
                    if (bits>>23)&255 != 255:
                        wide=int.from_bytes(struct.pack('<d',struct.unpack('<f',bits.to_bytes(4,'little'))[0]),'little')
                        doubles += [wide,(wide+1)&((1<<64)-1),(wide-1)&((1<<64)-1)]
                doubles += [rng.getrandbits(64) for _ in range(128)]
                for name,values,insize,outsize in [('ICVTRF',singles,4,8),('ICVTFR',doubles,8,4)]:
                    for bits in values:
                        magnitude=bits&((1<<(insize*8-1))-1)
                        exponent=(magnitude>>(23 if insize==4 else 52))
                        valid=(magnitude==0 or (0<exponent<255 if insize==4 else
                               897<=exponent<=1150 and (magnitude&((1<<29)-1))==0))
                        raw=bits.to_bytes(insize,'little')
                        if not rv:
                            lib.realresizeoracle.argtypes=[C.c_uint64,C.c_int,C.c_int,C.POINTER(C.c_uint64)]
                            for mode in range(4):
                                wanted=C.c_uint64()
                                flags=lib.realresizeoracle(bits,insize,mode,C.byref(wanted))
                                # Default ARM FPCR detects tininess before rounding; SSE detects it after.
                                if arch=='arm64' and insize==8 and 0<(bits&0x7fffffffffffffff)<0x3810000000000000 and flags&16:
                                    flags |= 8
                                for alias in (0,1,2,3,5):
                                    out,before,after,status=run(name,alias=alias,raw=raw,poison=True,fpstate=(mode,1))
                                    actual=int.from_bytes(out[:outsize],'little')
                                    limit=0x7f800000 if outsize==4 else 0x7ff0000000000000
                                    mask=(1<<(outsize*8-1))-1
                                    if wanted.value&mask>limit:assert actual&mask>limit
                                    else:assert actual==wanted.value,(arch,name,hex(bits),mode,hex(actual),hex(wanted.value))
                                    if arch=='arm64':assert status==flags,(arch,name,hex(bits),mode,status,flags)
                                    assert before[:8]==after[:8] and before[8+outsize:]==after[8+outsize:];checks+=1
                            continue
                        if valid:
                            number=struct.unpack('<f' if insize==4 else '<d',raw)[0]
                            expected=struct.pack('<d' if outsize==8 else '<f',number)
                        for alias in (0,1,2,3,5):
                            out,pc,before,after,_=run(name,alias=alias,raw=raw,expect=int(valid),poison=True)
                            if valid:
                                assert out[:outsize]==expected,(arch,wb,name,hex(bits),alias,out,expected)
                                assert before[:8]==after[:8] and before[8+outsize:]==after[8+outsize:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                        if not valid:
                            _,pc,before,after,_=run(name,raw=raw,expect=0,poison=True,badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                if case=='resize':continue
            if case in ('','integerreal'):
                values=[0,1,-1,127,-128,0x7fffffff,-0x80000000,0xffffffff,
                        (1<<52)-1,1<<52,(1<<53)-1,1<<53,(1<<53)+1,
                        -((1<<53)-1),-(1<<53),-((1<<53)+1),(1<<63)-1,-(1<<63)]
                values += [rng.randrange(-(1<<53)+1,1<<53) for _ in range(96)]
                values += [signed(rng.getrandbits(64),64) for _ in range(64)]
                for name,width in (('ICVTWF',wb),('ICVTLF',8)):
                    for value in values:
                        number=signed(value,width*8)
                        if not rv:
                            lib.integerrealoracle.argtypes=[C.c_int64,C.c_int,C.POINTER(C.c_uint64)]
                            for mode in range(4):
                                wanted=C.c_uint64()
                                flags=lib.integerrealoracle(number,mode,C.byref(wanted))
                                for alias in (0,1,2,3,5):
                                    out,before,after,status=run(name,value,alias=alias,poison=True,fpstate=(mode,1))
                                    assert int.from_bytes(out[:8],'little')==wanted.value,(arch,name,number,mode)
                                    if arch=='arm64':assert status==flags,(arch,name,number,mode,status,flags)
                                    assert before[:8]==after[:8] and before[16:]==after[16:];checks+=1
                            continue
                        valid=abs(number)<(1<<53)
                        for alias in (0,1,2,3,5):
                            out,pc,before,after,_=run(name,value,alias=alias,expect=int(valid),poison=True)
                            if valid:
                                assert out[:8]==struct.pack('<d',float(number)),(arch,wb,name,number,alias,out)
                                assert before[:8]==after[:8] and before[16:]==after[16:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                        if not valid:
                            _,pc,before,after,_=run(name,value,expect=0,poison=True,badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                if case=='integerreal':continue
            if case in ('','fixed'):
                # Fixed shifts use a signed 64-bit intermediate even in WORD32.
                values=[0,1,-1,127,-128,0x7fffffff,0x80000000,0xffffffff,
                        0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff]
                counts=list(range(-63,64))+[-64,64,65,-(1<<(wb*8-1)),(1<<(wb*8-1))-1]
                for value in values:
                    for count in counts:
                        valid=-63<=count<=63
                        for alias in (0,1,2,3,5):
                            out,pc,before,after,_=run('ICVTXX',value,count,alias=alias,expect=int(valid))
                            # d=m aliases preserve the same count; s=d-4/s=d+4 never overlaps m.
                            if valid:
                                expected=((signed(value,wb*8)<<count) if count>=0 else (signed(value,wb*8)>>-count))&((1<<(wb*8))-1)
                                assert int.from_bytes(out[:wb],'little')==expected,(arch,wb,value,count,alias,out)
                                assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                for count in (-64,64,65,-(1<<(wb*8-1))):
                    _,pc,before,after,_=run('ICVTXX',0,count,expect=0,badsource=True)
                    assert pc==oldpc and before==after
                    checks+=1
                if case=='fixed':continue
            if case in ('','scalar'):
                patterns=[0,1,127,128,255,256,0x8000,0xffff,0x7fffffff,0x80000000,
                          0xffffffff,0x100000000,0x8000000000000000,0xffffffffffffffff,
                          0x7ff0000000000042,0xfff8000000000042]
                for name,width in [('IMOVB',1),('IMOVW',wb),('IMOVL',8),('IMOVF',8),
                                   ('ICVTBW',wb),('ICVTWB',1),('ICVTWS',2),('ICVTSW',wb),
                                   ('ICVTWL',8),('ICVTLW',wb)]:
                    for bits in patterns:
                        raw=bits.to_bytes(8,'little')
                        wanted=(bits&255 if name=='ICVTBW' else
                                signed(bits,16) if name=='ICVTSW' else
                                signed(bits,wb*8) if name=='ICVTWL' else bits)
                        for alias in (0,1,3,5):
                            out,pc,before,after,_=run(name,alias=alias,raw=raw,poison=True)
                            assert int.from_bytes(out[:width],'little')==wanted&((1<<(8*width))-1),(arch,wb,name,bits,alias)
                            assert pc==oldpc and before[:8]==after[:8] and before[8+width:]==after[8+width:]
                            checks+=1
                if case=='scalar':continue
            if case in ('','realbranchfp'):
                if not rv:
                    patterns=(0,1<<63,1,0x8000000000000001,0xfffffffffffff,0x800fffffffffffff,
                              0x0010000000000000,0x3ff0000000000000,0xbff0000000000000,
                              0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000042,0x7ff0000000000042)
                    names=('IBEQF','IBNEF','IBLTF','IBLEF','IBGTF','IBGEF')
                    for a in patterns:
                        for b in patterns:
                            denormal=lambda x: 0<(x&0x7fffffffffffffff)<0x0010000000000000
                            nan=lambda x: (x&0x7fffffffffffffff)>0x7ff0000000000000
                            valid=not(nan(a) or nan(b))
                            for mode in range(4):
                                for flush in range(4):
                                    flushing=bool(flush&1)
                                    left=struct.unpack('<d',struct.pack('<Q',a& (1<<63) if flushing and denormal(a) else a))[0]
                                    right=struct.unpack('<d',struct.pack('<Q',b& (1<<63) if flushing and denormal(b) else b))[0]
                                    predicates=(left==right,left!=right,left<right,left<=right,left>right,left>=right)
                                    for condition,name in enumerate(names):
                                        for seed in range(2):
                                            take=valid and predicates[condition]
                                            pc,before,after,status=run(name,a,b,expect=int(valid),badtarget=not take,branchstate=(mode,flush,seed))
                                            assert pc==(target if take else oldpc) and before==after,(arch,wb,name,a,b,mode,flush)
                                            wanted=(4 if arch=='amd64' else 2) if seed else 0
                                            if valid and (denormal(a) or denormal(b)):
                                                if arch=='amd64' and not flushing:wanted|=2
                                                if arch=='arm64' and flushing:wanted|=128
                                            # Unicorn 2.1.4 omits the x86 denormal-operand sticky bit.
                                            # The physical AMD64 differential checks all MXCSR bits instead.
                                            if arch=='amd64':status&=~2;wanted&=~2
                                            assert status==wanted,(arch,name,hex(a),hex(b),flush,status,wanted)
                                            checks+=1
                if case=='realbranchfp':continue
            if case in ('','realbranch'):
                floats=[0,1<<63,0x3ff0000000000000,0xbff0000000000000,
                        0x0010000000000000,0x8010000000000000,0x7fefffffffffffff,0xffefffffffffffff,
                        0x7ff0000000000000,0xfff0000000000000,1,0x8000000000000001,
                        0xfffffffffffff,0x800fffffffffffff,0x7ff8000000000042,0xfff8000000000042,
                        0x7ff0000000000042,0xfff0000000000042]
                def native_real(bits):
                    exponent=(bits>>52)&2047
                    fraction=bits&((1<<52)-1)
                    return fraction==0 or 0<exponent<2047
                realpairs=[(a,b) for a in floats for b in floats]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(64)]
                for name,relation in [('IBEQF',0),('IBNEF',1),('IBLTF',2),('IBLEF',3),('IBGTF',4),('IBGEF',5)]:
                    for a,b in realpairs:
                        left=struct.unpack('<d',struct.pack('<Q',a))[0]
                        right=struct.unpack('<d',struct.pack('<Q',b))[0]
                        take=[left==right,left!=right,left<right,left<=right,left>right,left>=right][relation]
                        valid=(native_real(a) and native_real(b)) if rv else not (left!=left or right!=right)
                        _,pc,before,after,_=run(name,a,b,expect=int(valid))
                        assert pc==(target if valid and take else oldpc),(arch,wb,name,hex(a),hex(b),pc)
                        assert before==after
                        checks+=1
                        if not valid or not take:
                            _,pc,before,after,_=run(name,a,b,expect=int(valid),badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                    for a in floats:
                        if not native_real(a) and (rv or (a>>52)&2047==2047):
                            _,pc,before,after,_=run(name,a,poison=True,expect=0,badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                if case=='realbranch':continue
            if case in ('','headcopy'):
                for length in (0,1,2,3,7,8,15,16,31,32,63,64,127,128):
                    for delta in (-17,-1,0,1,4,17,129):
                        for slot in (0x204,0x500+layout.listdata+4,0x500+layout.listdata+16):
                            _,pc,_,_,_=run('IHEADM',headcopy=(data+0x500,length,delta,slot))
                            assert pc==oldpc
                            checks+=1
                for pointer in (0,(1<<64)-1):
                    for length in (-1,0,1,128,129,-(1<<(wb*8-1)),(1<<(wb*8-1))-1):
                        for poison in (False,True):
                            _,pc,before,after,_=run('IHEADM',headcopy=(pointer,length,0,0x204),expect=0,poison=poison)
                            assert pc==oldpc and before==after
                            checks+=1
                for length in (0,-1,129,256,-(1<<(wb*8-1)),(1<<(wb*8-1))-1):
                    _,pc,before,after,_=run('IHEADM',headcopy=(data+0x500,length,0,0x204),expect=int(length==0),poison=True)
                    assert pc==oldpc and before==after
                    checks+=1
                for length in (1,7,8,31,128):
                    for alias in (1,2):
                        _,pc,_,_,_=run('IHEADM',headcopy=(data+0x500,length,0,0x204),alias=alias)
                        assert pc==oldpc
                        checks+=1
                if case=='headcopy':continue
            if case in ('','power'):
                exponents=[-1,-2,-3,-(1<<(wb*8-1)),0,1,2,3,7,31,32,63,64,127,(1<<(wb*8-1))-1]
                if wb==8:exponents += [1<<32,(1<<32)+1]
                bases=[0,1,-1,2,-2,3,-3,(1<<31)-1,-(1<<31),(1<<63)-1,-(1<<63),0x123456789abcdef]
                for name,width in (('IEXPW',wb),('IEXPL',8)):
                    for exponent in exponents:
                        for base in bases:
                            for alias in (0,1,2,3,4):
                                out,pc,before,after,_=run(name,exponent,base,alias,expect=int(exponent>=0 or signed(base,width*8)!=0))
                                if exponent>=0 or signed(base,width*8)!=0:
                                    actual=signed(base,width*8)
                                    wanted=pow(base,exponent,1<<(width*8)) if exponent>=0 else (1 if actual==1 else ((-1 if exponent&1 else 1)&((1<<(width*8))-1)) if actual==-1 else 0)
                                    assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,exponent,base)
                                    assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                                else:assert before==after
                                assert pc==oldpc
                                checks+=1
                    for exponent in (-1,-(1<<(wb*8-1))):
                        _,pc,before,after,_=run(name,exponent,0,expect=0,badtarget=True)
                        assert pc==oldpc and before==after
                        checks+=1
                if case=='power':continue
            if case in ('','character'):
                values=[0,0x7f,0x80,0xff,0x10ffff,0x7fffffff,0x80000000,0xffffffff]
                for pointer in (0,(1<<64)-1,data+0x500):
                    for length in (-2147483648,-8,-1,0,1,8):
                        for index in [-1,0,1,7,8,(1<<(wb*8-1))-1]+([1<<32,(1<<32)+1,-(1<<63)] if wb==8 else []):
                            for alias in (0,1,2,3):
                                valid=pointer==data+0x500 and length!=-2147483648 and 0<=index<abs(length)
                                out,pc,before,after,_=run('IINDC',alias=alias,character=(pointer,length,index,values),expect=int(valid))
                                if not valid:assert before==after
                                assert pc==oldpc
                                checks+=1
                for length in (-8,8):
                    for index in (-1,8,(1<<(wb*8-1))-1):
                        _,pc,before,after,_=run('IINDC',character=(data+0x500,length,index,values),expect=0,poison=True)
                        assert before==after and pc==oldpc
                        checks+=1
                if case=='character':continue
            if case in ('','listlen'):
                for length in (0,1,2,7,31,127,128,129):
                    for terminal in (0,(1<<64)-1,data+0x500):
                        for alias in (0,1):
                            valid=length<=128 and terminal==(1<<64)-1
                            out,pc,before,after,_=run('ILENL',alias=alias,chain=(length,terminal),expect=int(valid))
                            if valid:
                                assert int.from_bytes(out[:wb],'little')==length
                                assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                if case=='listlen':continue
            if case in ('','head'):
                patterns=[0,1,(1<<64)-1,1<<63,0x7ff8000000000042,0x7ff0000000000042]+[rng.getrandbits(64) for _ in range(32)]
                for name in ('IHEADB','IHEADW','IHEADL','IHEADF'):
                    width=1 if name=='IHEADB' else wb if name=='IHEADW' else 8
                    for pointer in (0,(1<<64)-1,data+0x500):
                        for bits in patterns:
                            for alias in (0,1,2,3):
                                valid=pointer==data+0x500
                                out,pc,before,after,_=run(name,alias=alias,head=(pointer,bits),expect=int(valid))
                                if valid:assert int.from_bytes(out[:width],'little')==bits&((1<<(8*width))-1)
                                else:assert before==after
                                assert pc==oldpc
                                checks+=1
                if case=='head':continue
            if case in ('','string'):
                for pointer in (0,(1<<64)-1,data+0x500):
                    for length in (-2147483648,-2147483647,-65537,-1,0,1,65537,2147483647):
                        for alias in (0,1):
                            valid=pointer!=0 and (pointer==(1<<64)-1 or length!=-2147483648)
                            out,pc,before,after,_=run('ILENC',alias=alias,string=(pointer,length),expect=int(valid))
                            if valid:
                                wanted=0 if pointer==(1<<64)-1 else abs(length)
                                assert int.from_bytes(out[:wb],'little')==wanted,(arch,wb,pointer,length)
                                assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                if case=='string':continue
            if case in ('','copy'):
                for length in (0,1,2,3,7,8,15,16,31,32,63,64,127,128):
                    for delta in (-129,-64,-17,-8,-1,0,1,3,8,17,64,129):
                        for countslot in (0x704,0x304,0x308):
                            _,pc,_,_,_=run('IMOVM',copy=(0x304,0x304+delta,length,countslot))
                            assert pc==oldpc
                            checks+=1
                for length in (-1,-(1<<(wb*8-1)),129,256,(1<<(wb*8-1))-1):
                    _,pc,_,_,_=run('IMOVM',copy=(0x304,0x305,length,0x704),expect=0)
                    assert pc==oldpc
                    checks+=1
                if case=='copy':continue
            if case=='control':
                # Exercise signed-width boundaries and ensure false branches never read targets.
                relations={'EQ':lambda a,b:a==b,'NE':lambda a,b:a!=b,'LT':lambda a,b:a<b,
                           'LE':lambda a,b:a<=b,'GT':lambda a,b:a>b,'GE':lambda a,b:a>=b}
                edges=[0,1,127,128,255,256,0x7fffffff,0x80000000,0xffffffff,
                       0x100000000,0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff]
                for suffix,width in (('B',1),('W',wb),('L',8)):
                    for relation,predicate in relations.items():
                        name='IBNEW' if relation=='NE' and suffix=='W' else 'IB'+relation+suffix
                        for a in edges:
                            for b in edges:
                                aa=a&255 if width==1 else signed(a,width*8)
                                bb=b&255 if width==1 else signed(b,width*8)
                                take=predicate(aa,bb)
                                _,pc,before,after,_=run(name,a,b,badtarget=not take)
                                assert pc==(target if take else oldpc),(arch,wb,name,aa,bb)
                                assert before==after
                                checks+=1
                assert run('IJMP')[1]==target
                checks+=1
                continue
            if case=='divide':
                for suffix,width in (('B',1),('W',wb),('L',8)):
                    bits=width*8;mask=(1<<bits)-1
                    edges=[0,1,-1,2,-2,(1<<(bits-1))-1,-(1<<(bits-1)),(1<<bits)-1]
                    pairs=[(a,b) for a in edges for b in edges]
                    pairs += [(signed(rng.getrandbits(bits),bits),signed(rng.getrandbits(bits),bits)) for _ in range(64)]
                    for name in ('IDIV'+suffix,'IMOD'+suffix):
                        if not blobs[name]:
                            assert rv and not layout.hasdiv
                            continue
                        for divisor,dividend in pairs:
                            a=divisor&255 if width==1 else signed(divisor,bits)
                            b=dividend&255 if width==1 else signed(dividend,bits)
                            for alias in range(6):
                                out,pc,before,after,_=run(name,divisor,dividend,alias=alias,expect=int(a!=0))
                                if a:
                                    q=abs(b)//abs(a)
                                    if (a<0)!=(b<0):q=-q
                                    wanted=(q if name.startswith('IDIV') else b-q*a)&mask
                                    assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,a,b)
                                    assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                                else:assert before==after
                                assert pc==oldpc;checks+=1
                        _,pc,before,after,_=run(name,0,poison=True,expect=0)
                        assert before==after and pc==oldpc;checks+=1
                continue
            if case=='guard':
                counts=[-1,wb*8,-(1<<(wb*8-1))]+([1<<32] if wb==8 else [])
                for count in counts:
                    _,pc,before,after,_=run('ISHLW',count,poison=True,expect=0)
                    assert before==after and pc==oldpc
                    checks+=1
                continue
            owner=data+0x500
            for pointer in (0,(1<<64)-1,owner):
                for length in (-1,0,1,17,(1<<(wb*8-1))-1):
                    for alias in (0,1):
                        out,pc,before,after,_=run('ILENA',array=(pointer,length,0,0),arrayalias=alias)
                        wanted=(length if pointer==owner else 0)&((1<<(wb*8))-1)
                        assert int.from_bytes(out[:wb],'little')==wanted,(arch,wb,'ILENA',pointer,length,out,wanted)
                        assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:] and pc==oldpc
                        checks+=1
            for name in ('IINDB','IINDW','IINDL','IINDF'):
                stride=1 if name=='IINDB' else wb if name=='IINDW' else 8
                for pointer in (0,(1<<64)-1,owner):
                    for length in (-1,0,1,8,17,1<<40):
                        for index in (-1,0,1,7,8,16,17,1<<32,1<<39,-(1<<63)):
                            for backing in (0,(1<<64)-1,data+0x804,(1<<64)-2):
                                actualindex=signed(index,wb*8);actuallength=signed(length,wb*8)
                                valid=(pointer==owner and 0<=actualindex<actuallength and actualindex<=0x7fffffff//stride and
                                       backing not in (0,(1<<64)-1) and backing+actualindex*stride<(1<<64))
                                for alias in (0,1,2):
                                    out,pc,before,after,_=run(name,expect=int(valid),array=(pointer,length,index,backing),arrayalias=alias)
                                    if valid:
                                        assert int.from_bytes(out[:wb],'little')==(backing+actualindex*stride)&((1<<(8*wb))-1),(arch,name,length,index,backing,out)
                                        assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                                    else:assert before==after
                                    assert pc==oldpc
                                    checks+=1
            if case=='array':continue
            edges=[0,1,255,256,(1<<31)-1,1<<31,(1<<32)-1,1<<63,(1<<64)-1]
            pairs=[(a,b) for a in edges for b in edges]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(64)]
            for suffix,width in (('B',1),('W',wb),('L',8)):
                mask=(1<<(8*width))-1
                for operation in ('ADD','SUB','MUL','AND','OR','XOR'):
                    name='I'+operation+suffix
                    for a,b in pairs:
                        wanted={'ADD':lambda:b+a,'SUB':lambda:b-a,'MUL':lambda:b*a,
                                'AND':lambda:b&a,'OR':lambda:b|a,'XOR':lambda:b^a}[operation]()&mask
                        for alias in (0,1,2):
                            out,pc,before,after,_=run(name,a,b,alias)
                            assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,a,b,out,wanted)
                            assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                            assert pc==oldpc
                            checks+=1
            relations={'EQ':lambda a,b:a==b,'NE':lambda a,b:a!=b,'LT':lambda a,b:a<b,
                       'LE':lambda a,b:a<=b,'GT':lambda a,b:a>b,'GE':lambda a,b:a>=b}
            for suffix,width in (('B',1),('W',wb),('L',8)):
                for relation,predicate in relations.items():
                    name='IBNEW' if relation=='NE' and suffix=='W' else 'IB'+relation+suffix
                    for a,b in pairs[:81]:
                        _,pc,before,after,_=run(name,a,b)
                        aa=a&255 if width==1 else signed(a,width*8)
                        bb=b&255 if width==1 else signed(b,width*8)
                        assert pc==(target if predicate(aa,bb) else oldpc),(arch,wb,name,aa,bb,pc)
                        assert before==after
                        checks+=1
            for bits in [0,1,0x8000000000000000,0x3ff0000000000000,
                         0x7ff0000000000000,0xfff0000000000000,
                         0x7ff8000000000042,0x7ff0000000000042] + [rng.getrandbits(64) for _ in range(128)]:
                for alias in (0,1,3):
                    out,pc,before,after,_=run('INEGF',alias=alias,raw=struct.pack('<Q',bits))
                    assert int.from_bytes(out[:8],'little')==bits^(1<<63)
                    assert pc==oldpc and before[:8]==after[:8] and before[16:]==after[16:]
                    checks+=1
            for name,width in [('IMOVB' ,1),('IMOVW',wb),('IMOVL',8),('IMOVF',8),('ICVTBW',wb),('ICVTWB',1),('ICVTWS',2),('ICVTSW',wb),('ICVTWL',8),('ICVTLW',wb)]:
                for raw in (bytes.fromhex('efcdab8967452301'),bytes.fromhex('00800000ffffffff')):
                    wanted=(int.from_bytes(raw[:1],'little') if name=='ICVTBW' else
                            signed(int.from_bytes(raw[:2],'little'),16) if name=='ICVTSW' else
                            signed(int.from_bytes(raw[:wb],'little'),wb*8) if name=='ICVTWL' else int.from_bytes(raw[:width],'little'))
                    for alias in (0,1,3):
                        out,_,before,after,_=run(name,alias=alias,raw=raw)
                        assert int.from_bytes(out[:width],'little')==wanted&((1<<(8*width))-1),(arch,wb,name,out)
                        assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                        checks+=1
            for suffix,width in (('B',1),('W',wb),('L',8)):
                bits=width*8; mask=(1<<bits)-1
                shiftops=['ISHL'+suffix,'ISHR'+suffix]+(['ILSR'+suffix] if suffix!='B' else [])
                for name in shiftops:
                    counts=[-1,0,1,7,8,31,32,63,64,65,(1<<(wb*8-1))-1]
                    if wb==8: counts += [1<<32,(1<<32)+1,-(1<<63)]
                    for count in counts:
                        for value in edges:
                            valid=0<=count<bits
                            for alias in (0,1,2):
                                out,pc,before,after,_=run(name,count,value,alias,expect=int(valid))
                                if valid:
                                    wanted=((value<<count) if name.startswith('ISHL') else
                                            signed(value,bits)>>count if name.startswith('ISHR') and suffix!='B' else
                                            (value&mask)>>count)&mask
                                    assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,count,value,out,wanted)
                                    assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                                else: assert before==after
                                assert pc==oldpc
                                checks+=1
                        if not (0<=count<bits):
                            _,pc,before,after,_=run(name,count,poison=True,expect=0)
                            assert before==after and pc==oldpc
                            checks+=1
                for name in ('IDIV'+suffix,'IMOD'+suffix):
                    for divisor,dividend in pairs:
                        a=divisor&255 if width==1 else signed(divisor,bits)
                        b=dividend&255 if width==1 else signed(dividend,bits)
                        native=a!=0
                        for alias in (0,1,2):
                            out,pc,before,after,_=run(name,divisor,dividend,alias,expect=int(native))
                            if native:
                                q=abs(b)//abs(a)
                                if (a<0)!=(b<0):q=-q
                                wanted=(q if name.startswith('IDIV') else b-q*a)&mask
                                assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,a,b,out,wanted)
                                assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                            else: assert before==after
                            assert pc==oldpc
                            checks+=1
                    _,pc,before,after,_=run(name,0,poison=True,expect=0)
                    assert before==after and pc==oldpc
                    checks+=1
            assert run('IJMP')[1]==target
            out,_,_,_,s=run('ILEA'); assert int.from_bytes(out[:wb],'little')==s&((1<<(8*wb))-1)
        print('PASS',arch,'native leaves',flush=True)
print('PASS leaf64 JIT:',checks,'instruction/ABI/alias/oracle checks')
