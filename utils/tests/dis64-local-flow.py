#!/usr/bin/env python3
"""Check private control IR ownership and call graphs, without enabling wire calls."""
import argparse
import os
from pathlib import Path
import random
import re
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NORMAL = {'IMOVW','IMOVL','IMOVF','IFRAME','ILEA'} | {'I'+op+suffix for op in ('ADD','SUB','MUL','DIV','MOD','AND','OR','XOR','SHL','SHR') for suffix in ('W','L')}
BRANCH = {'I'+op+suffix for op in ('BEQ','BNE','BLT','BLE','BGT','BGE') for suffix in ('W','L','F')}
TERMINAL = {'IRET','IEXIT'}


# Independently traverse the Python call graph with DFS, not the C topological queue.
def oracle(functions, nodes, limit):
    if not functions or len(functions)>64 or not nodes or len(nodes)>65536 or not 1<=limit<=64:
        return None
    end=0
    for first,last in functions:
        if first!=end or first>=last or last>len(nodes):return None
        end=last
    if end!=len(nodes):return None
    entries={first:i for i,(first,last) in enumerate(functions)}
    graph=[set() for _ in functions]
    branches=calls=terminals=0
    for owner,(first,last) in enumerate(functions):
        for pc in range(first,last):
            op,target=nodes[pc]
            if op in TERMINAL:
                if target:return None
                terminals+=1
                continue
            if op not in NORMAL|BRANCH|{'IJMP','ICALL'}:return None
            if op!='IJMP' and pc+1==last:return None
            if op in BRANCH|{'IJMP'}:
                if not first<=target<last:return None
                branches+=1
            elif op=='ICALL':
                if target not in entries:return None
                graph[owner].add(entries[target]);calls+=1
            elif target:return None
    active=set();depths={}

    # A back edge anywhere, even in a disconnected declaration, refuses the draft subset.
    def visit(owner):
        if owner in active:raise ValueError('cycle')
        if owner in depths:return depths[owner]
        active.add(owner)
        depth=1+max((visit(child) for child in graph[owner]),default=0)
        active.remove(owner);depths[owner]=depth
        return depth

    try:depth=max(visit(owner) for owner in range(len(functions)))
    except ValueError:return None
    if depth>limit:return None
    return [len(functions),len(nodes),branches,calls,depth,terminals]


# Enumerate boundary, backward-edge, interior-call, disconnected-cycle and depth cases.
def cases():
    result=[]
    basefn=[(0,4),(4,8)]
    baseline=[('IMOVW',0),('ICALL',4),('IMOVL',0),('IEXIT',0),('IMOVF',0),('IMOVW',0),('IMOVL',0),('IRET',0)]
    result.append((basefn,baseline,64))
    for owner in range(2):
        first,last=basefn[owner]
        for op in sorted(BRANCH|{'IJMP','ICALL'}):
            for target in (0,first,last-1,last,7,8,0xffffffff):
                for pc in (first,last-1):
                    nodes=baseline.copy();nodes[pc]=(op,target)
                    result.append((basefn,nodes,64))
    for op in sorted(NORMAL|TERMINAL|{'INOP','IALT','ILOAD','UNKNOWN'}):
        for pc in (0,3,4,7):
            for target in (0,1):
                nodes=baseline.copy();nodes[pc]=(op,target)
                result.append((basefn,nodes,64))
    for count in range(1,65):
        functions=[(4*i,4*i+4) for i in range(count)]
        nodes=[('IMOVW',0)]*(4*count)
        for i in range(count):
            nodes[4*i+3]=('IRET',0)
            if i+1<count:nodes[4*i]=('ICALL',4*(i+1))
        for limit in {1,count,max(1,count-1),64}:
            result.append((functions,nodes.copy(),limit))
        if count>1:
            cycle=nodes.copy();cycle[4*(count-1)]=('ICALL',0)
            result.append((functions,cycle,64))
    # Repeated call sites must not inflate a callee's indegree.
    result.append(([(0,4),(4,6)],[('ICALL',4),('ICALL',4),('ICALL',4),('IRET',0),('IMOVW',0),('IRET',0)],2))
    # Root has no calls; the disconnected helper's self-cycle still refuses.
    result.append(([(0,1),(1,3)],[('IEXIT',0),('ICALL',1),('IRET',0)],64))
    result.append(([(0,1)],[('IJMP',0)],1))
    for functions in ([],[(1,8)],[(0,0),(0,8)],[(0,3),(4,8)],[(0,5),(4,8)],[(0,9)]):
        result.append((functions,baseline.copy(),64))
    for limit in (0,65,0xffffffff):result.append((basefn,baseline.copy(),limit))
    rng=random.Random(74)
    for _ in range(400):
        count=rng.randrange(1,12)
        functions=[(5*i,5*i+5) for i in range(count)]
        nodes=[]
        for owner,(first,last) in enumerate(functions):
            for pc in range(first,last-1):
                op=rng.choice(['IMOVW','IBEQW','IJMP','ICALL'])
                target=rng.randrange(len(functions)*5) if op!='IMOVW' else 0
                nodes.append((op,target))
            nodes.append(('IRET',0))
        result.append((functions,nodes,rng.randrange(1,65)))
    result.append(([],[],1))
    result.append(([(i,i+1) for i in range(65)],[('IRET',0)]*65,64))
    result.append(([(0,65537)],[('IMOVW',0)]*65536+[('IEXIT',0)],1))
    result.append(([(0,65536)],[('IMOVW',0)]*65535+[('IEXIT',0)],1))
    return result


# Compile the same control checker for each target and require exact semantic controls.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control',choices=('branch','fallthrough','calls','cycle','depth'))
    args=parser.parse_args()
    env=dict(os.environ)
    compiler=shlex.split(env.get('CC','cc'))
    flags=shlex.split(env.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    runner=shlex.split(env.get('NATIVE64_RUNNER',''))
    isa=(ROOT/'include/isa.h').read_text().split('};',1)[0]
    codes={name:i for i,name in enumerate(re.findall(r'^\s*(I[A-Z0-9]+),',isa,re.M))}
    words=lambda *v:struct.pack('>'+'I'*len(v),*v)
    inputs=cases();data=words(len(inputs));positive=0
    for functions,nodes,limit in inputs:
        expected=oracle(functions,nodes,limit)
        positive+=expected is not None
        data+=words(len(functions),len(nodes),limit,int(expected is not None))
        data+=b''.join(words(*span) for span in functions)
        data+=b''.join(words(codes.get(op,255),target) for op,target in nodes)
        if expected is not None:data+=words(*expected)
    with tempfile.TemporaryDirectory(prefix='dis64-local-flow-',dir=env.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);env['TMPDIR']=str(work)
        header=(ROOT/'include/dis64-local-flow.h').read_text()
        mutations={
            'branch':('if(target < first || target >= end)','if(0)'),
            'fallthrough':('if(kind != Dis64FlowJump && pc+1 == end)','if(0)'),
            'calls':('j<count && functions[j].entry != target','j<count && !(target >= functions[j].entry && target < functions[j].end)'),
            'cycle':('if(seen != count || longest > depthlimit)','if(longest > depthlimit)'),
            'depth':('if(seen != count || longest > depthlimit)','if(seen != count)')}
        if args.control:
            old,new=mutations[args.control]
            if header.count(old)!=1:raise RuntimeError('mutation marker drift')
            header=header.replace(old,new)
        (work/'dis64-local-flow.h').write_text(header)
        fixture=work/'cases.bin';fixture.write_bytes(data)
        binary=work/'flow'
        subprocess.run(compiler+flags+['-I'+str(work),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-local-flow.c'),'-o',str(binary)],env=env,check=True)
        run=subprocess.run(runner+[str(binary),str(fixture)],env=env,capture_output=True,text=True,timeout=90)
        if args.control:
            if run.returncode!=10 or 'local flow predicate failed' not in run.stderr:
                raise RuntimeError('flow control missed predicate: '+run.stdout+run.stderr)
            print('PASS experimental flow removal control:',args.control)
        elif run.returncode:raise RuntimeError(run.stdout+run.stderr)
        else:print(run.stdout,end='');print('Independent oracle:',positive,'accepted,',len(inputs)-positive,'refused')


if __name__=='__main__':
    main()
