"""Independent raw scalar DAG oracle; leaf CFGs use the older return oracle."""
import struct

def check(C, case):
    code,exports,pool,types,objlimit,entry,entryframe,locals_,workcap,capacity,limit=case
    try:
        decoded=C.decode(code);r=C.BASE.Reader(exports);bindings=[]
        for _ in range(r.word()):
            symbol,pc,frame,length=[r.word()for _ in range(4)];sig=r.take(length)
            bindings.append((symbol,pc,frame,sig))
        bindings.sort(key=lambda x:x[1]);n=len(bindings)
        if not n or n>64:return False
        ends=[x[1]for x in bindings[1:]]+[len(decoded)]
        callers=[i==0 or any(x[0]in ('IFRAME','ICALL')for x in decoded[b[1]:end])for i,(b,end)in enumerate(zip(bindings,ends))]
        # Erase only bodies checked below. Original leaf CFG bytes and parameters
        # remain intact. Erased helper results become void in this projection.
        proxy=[x[-1]for x in decoded];projected=struct.pack('>I',n)
        for i,((symbol,first,frame,sig),end)in enumerate(zip(bindings,ends)):
            if callers[i]:
                proxy[first:end]=[C.EXIT if i==0 else C.RET]*(end-first)
                sig=sig[:4]+bytes(4)+sig[8:]
            projected+=struct.pack('>IIII',symbol,first,frame,len(sig))+sig
        if C.RETURNS.oracle((C.IMAGE(proxy),projected,*case[2:10],C.RETURNS.LIMIT))is None:return False
        tr=C.BASE.Reader(types);frames=[]
        for _ in range(tr.word()):
            size,align=tr.word(),tr.word();tr.take((size+63)//64);frames.append(size)
        entries={b[1]:i for i,b in enumerate(bindings)};edges=[set()for _ in bindings]
        for owner,((symbol,first,frame,sig),end)in enumerate(zip(bindings,ends)):
            if not callers[owner]:continue
            slots=next(x[3]for x in locals_ if x[:3]==(symbol,first,frame))
            words=struct.unpack('>'+'I'*(len(sig)//4),sig);result,kinds=words[1],words[3:]
            assigned=set();at=64
            for kind in kinds:
                width=C.OP.WIDTH[kind];at=(at+width-1)//width*width
                assigned.add(slots.index((at,kind))+1);at+=width
            tail=frames[frame]-8;ready=0;stored=False;pc=first
            def read(mode,value,kind):
                if mode==2:
                    if kind==3:raise ValueError('real literal')
                    return 0
                if mode!=1:raise ValueError('address role')
                ident=slots.index((value,kind))+1
                if ident not in assigned:raise ValueError('uninitialized')
                return ident
            def write(value,kind):assigned.add(slots.index((value,kind))+1)
            while pc<end:
                name,mode,source,middle,destination,secondary,raw=decoded[pc]
                if name=='IFRAME':
                    if mode!=17 or destination not in (40,48,56):return False
                    pointer=destination;start=pc;pc+=1
                    while pc<end and decoded[pc][0]not in ('IFRAME','ICALL','IRET','IEXIT'):pc+=1
                    if pc==end or decoded[pc][0]!='ICALL':return False
                    _,cm,cp,_,target,_,_=decoded[pc];chosen=entries.get(target,0)
                    if not chosen or cm!=10 or cp!=pointer or bindings[chosen][2]!=source:return False
                    edges[owner].add(chosen)
                    sw=struct.unpack('>'+'I'*(len(bindings[chosen][3])//4),bindings[chosen][3]);ck=sw[1];args=sw[3:]
                    offsets={};at=64
                    for index,kind in enumerate(args):
                        width=C.OP.WIDTH[kind];at=(at+width-1)//width*width;offsets[at]=(index,kind);at+=width
                    filled=set();bound=False
                    for ins in decoded[start+1:pc]:
                        op,m,s,h,d,extra,_=ins
                        if d!=pointer or m&7!=5 or m&192:return False
                        if op=='ILEA':
                            if m!=13 or s!=tail or extra!=32 or not ck or bound:return False
                            bound=True
                        else:
                            index,kind=offsets[extra]
                            if index in filled or op!={1:'IMOVW',2:'IMOVL',3:'IMOVF'}[kind]:return False
                            read(m>>3&7,s,kind);filled.add(index)
                    if len(filled)!=len(args)or bool(ck)!=bound:return False
                    ready=ck
                elif mode&7==5:
                    if owner==0 or destination!=32 or secondary or mode&192 or not result or name!={1:'IMOVW',2:'IMOVL',3:'IMOVF'}[result]:return False
                    read(mode>>3&7,source,result);stored=True
                elif name in C.OP.MOVES and mode>>3&7==1 and source==tail:
                    if not ready or name!={1:'IMOVW',2:'IMOVL',3:'IMOVF'}[ready]or mode!=9:return False
                    write(destination,ready)
                elif name in ('ICVTWF','ICVTWL'):
                    if mode not in (9,17):return False
                    read(mode>>3&7,source,1);write(destination,2 if name=='ICVTWL'else 3)
                elif name in ('IRET','IEXIT'):
                    if name!=('IRET'if owner else'IEXIT')or mode!=27 or pc+1!=end or result and not stored:return False
                else:
                    facts=C.OP.oracle(frames[frame],slots,C.IMAGE([raw,C.EXIT]),2)
                    if facts is None or facts[0][7]!=0:return False
                    f=facts[0]
                    if any(x and x not in assigned for x in f[2:4]):return False
                    if f[4]:assigned.add(f[4])
                pc+=1
            if decoded[end-1][0]!=('IRET'if owner else'IEXIT'):return False
        # DFS proves graph acyclicity independently of production's bottom-up scan.
        active=set();finished=set()
        def visit(i):
            if i in active:return False
            if i in finished:return True
            active.add(i)
            if not all(visit(j)for j in edges[i]):return False
            active.remove(i);finished.add(i);return True
        return all(visit(i)for i in range(n))
    except (ValueError,IndexError,KeyError,TypeError,struct.error):return False
