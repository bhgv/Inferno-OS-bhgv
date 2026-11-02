#!/usr/bin/env python3
"""Execute restricted source helpers without admitting a file calling ABI."""
from pathlib import Path
import argparse
import json
import re
import runpy
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HELPERS = runpy.run_path(str(ROOT/'utils/tests/dis64-cases.py'))
PREFIX = 'implement InlineProbe;\nInlineProbe: module {init: fn();};\n'


# Retain source calls in ordinary IR while checking effects with independent constant answers.
def programs():
    yield 'helper-expr-int-0', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x+3);if(value!=8)value/=0;}', ''
    yield 'helper-expr-big-0', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x+big 3);if(value!=big 8)value/=big 0;}', ''
    yield 'helper-expr-int-1', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x-3);if(value!=2)value/=0;}', ''
    yield 'helper-expr-big-1', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x-big 3);if(value!=big 2)value/=big 0;}', ''
    yield 'helper-expr-int-2', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x*3);if(value!=15)value/=0;}', ''
    yield 'helper-expr-big-2', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x*big 3);if(value!=big 15)value/=big 0;}', ''
    yield 'helper-expr-int-3', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x/3);if(value!=1)value/=0;}', ''
    yield 'helper-expr-big-3', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x/big 3);if(value!=big 1)value/=big 0;}', ''
    yield 'helper-expr-int-4', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x%3);if(value!=2)value/=0;}', ''
    yield 'helper-expr-big-4', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x%big 3);if(value!=big 2)value/=big 0;}', ''
    yield 'helper-expr-int-5', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x&3);if(value!=1)value/=0;}', ''
    yield 'helper-expr-big-5', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x&big 3);if(value!=big 1)value/=big 0;}', ''
    yield 'helper-expr-int-6', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x|3);if(value!=7)value/=0;}', ''
    yield 'helper-expr-big-6', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x|big 3);if(value!=big 7)value/=big 0;}', ''
    yield 'helper-expr-int-7', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x^3);if(value!=6)value/=0;}', ''
    yield 'helper-expr-big-7', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x^big 3);if(value!=big 6)value/=big 0;}', ''
    yield 'helper-expr-int-8', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x<<3);if(value!=40)value/=0;}', ''
    yield 'helper-expr-big-8', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x<<3);if(value!=big 40)value/=big 0;}', ''
    yield 'helper-expr-int-9', 'x:=5;value:int;', 'helper(a:int){value+=a;} init(){helper(x>>3);if(value!=0)value/=0;}', ''
    yield 'helper-expr-big-9', 'x:=big 5;value:big;', 'helper(a:big){value+=a;} init(){helper(x>>3);if(value!=big 0)value/=big 0;}', ''
    yield 'helper-expr-zero', 'x:=5;z:int;value:int;', 'helper(a:int){value+=a;} init(){helper(x/z);}', 'zero divide'
    yield 'helper-expr-shift', 'x:=5;z:=32;value:int;', 'helper(a:int){value+=a;} init(){helper(x<<z);}', 'invalid argument'
    yield 'helper-expr-nested-word', 'x:=5;value:int;', 'inner(a:int){value+=a;} helper(a:int){inner(a+3);} init(){helper(x);if(value!=8)value/=0;}', ''
    yield 'helper-expr-nested-big', 'x:=big 5;value:big;', 'inner(a:big){value+=a;} helper(a:big){inner(a+big 3);} init(){helper(x);if(value!=big 8)value/=big 0;}', ''
    yield 'helper-expr-multiple', 'x:=5;value:int;', 'helper(a:int,b:int){value=a+b;} init(){helper(x+3,x*2);if(value!=18)value/=0;}', ''
    yield 'helper-expr-local', 'x:=5;value:int;', 'inner(a:int){value+=a;} helper(){a:=x;a++;inner(a*3);if(a!=6)value/=0;} init(){helper();if(value!=18)value/=0;}', ''
    yield 'helper-result-int-8-+', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a+3;} init(){answer=helper(x);if(answer!=8||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-2--', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a-3;} init(){answer=helper(x);if(answer!=2||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-15-*', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a*3;} init(){answer=helper(x);if(answer!=15||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-1-/', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a/3;} init(){answer=helper(x);if(answer!=1||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-2-%', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a%3;} init(){answer=helper(x);if(answer!=2||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-1-&', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a&3;} init(){answer=helper(x);if(answer!=1||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-7-|', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a|3;} init(){answer=helper(x);if(answer!=7||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-6-^', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a^3;} init(){answer=helper(x);if(answer!=6||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-40-<<', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a<<3;} init(){answer=helper(x);if(answer!=40||calls!=1)calls/=0;}', ''
    yield 'helper-result-int-0->>', 'calls:int;x:=5;answer:int;', 'helper(a:int):int{calls++;return a>>3;} init(){answer=helper(x);if(answer!=0||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-8-+', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a+big 3;} init(){answer=helper(x);if(answer!=big 8||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-2--', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a-big 3;} init(){answer=helper(x);if(answer!=big 2||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-15-*', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a*big 3;} init(){answer=helper(x);if(answer!=big 15||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-1-/', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a/big 3;} init(){answer=helper(x);if(answer!=big 1||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-2-%', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a%big 3;} init(){answer=helper(x);if(answer!=big 2||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-1-&', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a&big 3;} init(){answer=helper(x);if(answer!=big 1||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-7-|', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a|big 3;} init(){answer=helper(x);if(answer!=big 7||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-6-^', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a^big 3;} init(){answer=helper(x);if(answer!=big 6||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-40-<<', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a<<3;} init(){answer=helper(x);if(answer!=big 40||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-0->>', 'calls:int;x:=big 5;answer:big;', 'helper(a:big):big{calls++;return a>>3;} init(){answer=helper(x);if(answer!=big 0||calls!=1)calls/=0;}', ''
    yield 'helper-result-original', 'value:int;', 'helper():int{value++;return value;} init(){value=helper();if(value!=1)value/=0;}', ''
    yield 'helper-result-real', 'calls:int;answer:real;', 'helper(a:real):real{calls++;return a;} init(){answer=helper(1.25);if(answer!=1.25||calls!=1)calls/=0;}', ''
    yield 'helper-result-local', 'calls:int;x:=7;', 'helper(a:int):int{calls++;return a+1;} init(){answer:=helper(x);if(answer!=8||calls!=1)calls/=0;}', ''
    yield 'helper-result-big-local', 'calls:int;x:=big 7;', 'helper(a:big):big{calls++;return a+big 1;} init(){answer:=helper(x);if(answer!=big 8||calls!=1)calls/=0;}', ''
    yield 'helper-result-real-local', 'calls:int;x:=1.25;', 'helper(a:real):real{calls++;return a;} init(){answer:=helper(x);if(answer!=1.25||calls!=1)calls/=0;}', ''
    yield 'helper-result-discard', 'calls:int;', 'helper():int{calls++;return calls;} init(){helper();if(calls!=1)calls/=0;}', ''
    yield 'helper-result-nested', 'calls:int;answer:int;', 'inner(a:int):int{calls++;return a+1;} helper(a:int):int{calls++;return inner(a)+3;} init(){answer=helper(7);if(answer!=11||calls!=2)calls/=0;}', ''
    yield 'helper-result-forward', 'calls:int;answer:int;', 'inner(a:int):int{calls++;return a+1;} helper(a:int):int{calls++;return inner(a);} init(){answer=helper(7);if(answer!=8||calls!=2)calls/=0;}', ''
    yield 'helper-result-big-forward', 'calls:int;answer:big;', 'inner(a:big):big{calls++;return a+big 1;} helper(a:big):big{calls++;return inner(a);} init(){answer=helper(big 7);if(answer!=big 8||calls!=2)calls/=0;}', ''
    yield 'helper-result-real-forward', 'calls:int;answer:real;', 'inner(a:real):real{calls++;return a;} helper(a:real):real{calls++;return inner(a);} init(){answer=helper(1.25);if(answer!=1.25||calls!=2)calls/=0;}', ''
    yield 'helper-result-repeat', 'calls:int;x:=7;', 'helper(a:int):int{local:int;local++;calls++;return a+local;} init(){for(i:=0;i<4097;i++){answer:=helper(x);if(answer!=8)calls/=0;}if(calls!=4097)calls/=0;}', ''
    yield 'helper-result-zero-divide', 'calls:int;x:=7;z:int;', 'helper(a:int):int{\ncalls++;\nreturn a/z;\n}\ninit(){answer:=helper(x);}', 'zero divide'
    yield 'helper-result-invalid-shift', 'calls:int;x:=7;z:=32;', 'helper(a:int):int{\ncalls++;\nreturn a<<z;\n}\ninit(){answer:=helper(x);}', 'invalid argument'
    yield 'helper-result-early-0', 'calls:int;key:=0;answer:int;', 'helper(a:int):int{calls++;if(a==0)return 11;return a+1;} init(){answer=helper(key);if(answer!=11||calls!=1)calls/=0;}', ''
    yield 'helper-result-early-7', 'calls:int;key:=7;answer:int;', 'helper(a:int):int{calls++;if(a==0)return 11;return a+1;} init(){answer=helper(key);if(answer!=8||calls!=1)calls/=0;}', ''
    yield 'helper-result-early-99', 'calls:int;key:=99;answer:int;', 'helper(a:int):int{calls++;if(a==0)return 11;return a+1;} init(){answer=helper(key);if(answer!=100||calls!=1)calls/=0;}', ''
    yield 'helper-result-case-0', 'calls:int;key:=0;answer:int;', 'helper(a:int):int{calls++;case a {0=>return 11;1 to 10=>return 19;* =>return 23;}} init(){answer=helper(key);if(answer!=11||calls!=1)calls/=0;}', ''
    yield 'helper-result-case-7', 'calls:int;key:=7;answer:int;', 'helper(a:int):int{calls++;case a {0=>return 11;1 to 10=>return 19;* =>return 23;}} init(){answer=helper(key);if(answer!=19||calls!=1)calls/=0;}', ''
    yield 'helper-result-case-99', 'calls:int;key:=99;answer:int;', 'helper(a:int):int{calls++;case a {0=>return 11;1 to 10=>return 19;* =>return 23;}} init(){answer=helper(key);if(answer!=23||calls!=1)calls/=0;}', ''
    yield 'helper-result-control', 'calls:int;x:=3;', '\nhelper():int{\ncalls++;\nreturn x+calls;\n}\ninit(){answer:=helper();if(answer!=4||calls!=1)calls/=0;}', ''
    yield 'helper-result-argument-call', 'value:int;', 'inner():int{value++;return value;} helper(a:int){value+=a;} init(){helper(inner());if(value!=2)value/=0;}', ''
    yield 'helper-tuple-shape-0', 'calls:int;x:=7;', 'helper(a:int):(int,int){calls++;return(a+1,a+1);} init(){(v0,v1):=helper(x);if(v0!=8||v1!=8||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-1', 'calls:int;x:=7;', 'helper(a:int):(int,big){calls++;return(a+1,big 17);} init(){(v0,v1):=helper(x);if(v0!=8||v1!=big 17||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-2', 'calls:int;x:=7;', 'helper(a:int):(int,real){calls++;return(a+1,1.25);} init(){(v0,v1):=helper(x);if(v0!=8||v1!=1.25||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-3', 'calls:int;x:=7;', 'helper(a:int):(big,int){calls++;return(big 17,a+1);} init(){(v0,v1):=helper(x);if(v0!=big 17||v1!=8||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-4', 'calls:int;x:=7;', 'helper(a:int):(big,big){calls++;return(big 17,big 17);} init(){(v0,v1):=helper(x);if(v0!=big 17||v1!=big 17||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-5', 'calls:int;x:=7;', 'helper(a:int):(big,real){calls++;return(big 17,1.25);} init(){(v0,v1):=helper(x);if(v0!=big 17||v1!=1.25||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-6', 'calls:int;x:=7;', 'helper(a:int):(real,int){calls++;return(1.25,a+1);} init(){(v0,v1):=helper(x);if(v0!=1.25||v1!=8||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-7', 'calls:int;x:=7;', 'helper(a:int):(real,big){calls++;return(1.25,big 17);} init(){(v0,v1):=helper(x);if(v0!=1.25||v1!=big 17||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-8', 'calls:int;x:=7;', 'helper(a:int):(real,real){calls++;return(1.25,1.25);} init(){(v0,v1):=helper(x);if(v0!=1.25||v1!=1.25||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-9', 'calls:int;x:=7;', 'helper(a:int):(int,big,real){calls++;return(a+1,big 17,1.25);} init(){(v0,v1,v2):=helper(x);if(v0!=8||v1!=big 17||v2!=1.25||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-10', 'calls:int;x:=7;', 'helper(a:int):(int,real,big){calls++;return(a+1,1.25,big 17);} init(){(v0,v1,v2):=helper(x);if(v0!=8||v1!=1.25||v2!=big 17||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-11', 'calls:int;x:=7;', 'helper(a:int):(big,int,real){calls++;return(big 17,a+1,1.25);} init(){(v0,v1,v2):=helper(x);if(v0!=big 17||v1!=8||v2!=1.25||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-12', 'calls:int;x:=7;', 'helper(a:int):(big,real,int){calls++;return(big 17,1.25,a+1);} init(){(v0,v1,v2):=helper(x);if(v0!=big 17||v1!=1.25||v2!=8||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-13', 'calls:int;x:=7;', 'helper(a:int):(real,int,big){calls++;return(1.25,a+1,big 17);} init(){(v0,v1,v2):=helper(x);if(v0!=1.25||v1!=8||v2!=big 17||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-shape-14', 'calls:int;x:=7;', 'helper(a:int):(real,big,int){calls++;return(1.25,big 17,a+1);} init(){(v0,v1,v2):=helper(x);if(v0!=1.25||v1!=big 17||v2!=8||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-forward-1', 'calls:int;x:=7;', 'leaf(a:int):(int,big,real){calls++;return(a+1,big 17,1.25);}layer0(a:int):(int,big,real){calls++;return leaf(a);}init(){(u,v,w):=layer0(x);if(u!=8||v!=big 17||w!=1.25||calls!=2)calls/=0;}', ''
    yield 'helper-tuple-forward-2', 'calls:int;x:=7;', 'leaf(a:int):(int,big,real){calls++;return(a+1,big 17,1.25);}layer0(a:int):(int,big,real){calls++;return leaf(a);}layer1(a:int):(int,big,real){calls++;return layer0(a);}init(){(u,v,w):=layer1(x);if(u!=8||v!=big 17||w!=1.25||calls!=3)calls/=0;}', ''
    yield 'helper-tuple-forward-3', 'calls:int;x:=7;', 'leaf(a:int):(int,big,real){calls++;return(a+1,big 17,1.25);}layer0(a:int):(int,big,real){calls++;return leaf(a);}layer1(a:int):(int,big,real){calls++;return layer0(a);}layer2(a:int):(int,big,real){calls++;return layer1(a);}init(){(u,v,w):=layer2(x);if(u!=8||v!=big 17||w!=1.25||calls!=4)calls/=0;}', ''
    yield 'helper-tuple-nested', 'calls:int;x:=7;', 'inner(a:int):(int,big){calls++;return(a,big 17);} helper(a:int):(int,big){calls++;(u,v):=inner(a);return(u+1,v+big 2);} init(){(u,v):=helper(x);if(u!=8||v!=big 19||calls!=2)calls/=0;}', ''
    yield 'helper-tuple-loop', 'calls:int;x:=7;', 'helper(a:int):(int,int){local:int;local++;calls++;return(a,local);} init(){for(i:=0;i<4097;i++){(u,v):=helper(x);if(u!=7||v!=1)calls/=0;}if(calls!=4097)calls/=0;}', ''
    yield 'helper-tuple-discard', 'calls:int;x:=7;', 'helper(a:int):(int,big,real){calls++;return(a,big 17,1.25);} init(){helper(x);if(calls!=1)calls/=0;}', ''
    yield 'helper-tuple-control', 'calls:int;x:=7;', '\nhelper():(int,int){\ncalls++;\nreturn (x+1,x+2);\n}\ninit(){(u,v):=helper();if(u!=8||v!=9||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-early-0', 'calls:int;key:=0;', 'helper(a:int):(int,big){calls++;if(a==0)return(11,big 12);return(a,big 17);} init(){(u,v):=helper(key);if(u!=11||v!=big 12||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-early-7', 'calls:int;key:=7;', 'helper(a:int):(int,big){calls++;if(a==0)return(11,big 12);return(a,big 17);} init(){(u,v):=helper(key);if(u!=7||v!=big 17||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-early-99', 'calls:int;key:=99;', 'helper(a:int):(int,big){calls++;if(a==0)return(11,big 12);return(a,big 17);} init(){(u,v):=helper(key);if(u!=99||v!=big 17||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-case-0', 'calls:int;key:=0;', 'helper(a:int):(int,big){calls++;case a {0=>return(1,big 11);1 to 10=>return(2,big 22);* =>return(3,big 33);}} init(){(u,v):=helper(key);if(u!=1||v!=big 11||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-case-7', 'calls:int;key:=7;', 'helper(a:int):(int,big){calls++;case a {0=>return(1,big 11);1 to 10=>return(2,big 22);* =>return(3,big 33);}} init(){(u,v):=helper(key);if(u!=2||v!=big 22||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-case-99', 'calls:int;key:=99;', 'helper(a:int):(int,big){calls++;case a {0=>return(1,big 11);1 to 10=>return(2,big 22);* =>return(3,big 33);}} init(){(u,v):=helper(key);if(u!=3||v!=big 33||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-boundary', 'calls:int;', 'helper():(big,big,big,big,big,big,big,big){calls++;return(big 1,big 2,big 3,big 4,big 5,big 6,big 7,big 8);} init(){(v0,v1,v2,v3,v4,v5,v6,v7):=helper();if(v0!=big 1||v1!=big 2||v2!=big 3||v3!=big 4||v4!=big 5||v5!=big 6||v6!=big 7||v7!=big 8||calls!=1)calls/=0;}', ''
    yield 'helper-tuple-fault-word', 'calls:int;z:int;x:=7;', 'helper(a:int):(int,int){\ncalls++;\nreturn (a,a/z);\n}\ninit(){(u,v):=helper(x);}', 'zero divide'
    yield 'helper-tuple-fault-big', 'calls:int;z:big;x:=7;', 'helper(a:int):(int,big){\ncalls++;\nreturn (a,big 17/z);\n}\ninit(){(u,v):=helper(x);}', 'zero divide'
    yield 'helper-tuple-fault-shift', 'calls:int;z:=32;x:=7;', 'helper(a:int):(int,int){\ncalls++;\nreturn (a,a<<z);\n}\ninit(){(u,v):=helper(x);}', 'invalid argument'
    yield 'helper-tuple-fault-forward', 'calls:int;z:big;x:=7;', 'inner(a:int):(int,big){\ncalls++;\nreturn (a,big 17/z);\n}\nhelper(a:int):(int,big){calls++;return inner(a);}\ninit(){(u,v):=helper(x);}', 'zero divide'
    yield 'before-entry', 'value:=17;', 'helper(){value+=3;}\ninit(){helper();helper();if(value!=23)value/=0;}', ''
    yield 'after-entry', 'value:=17;', 'init(){helper();if(value!=20)value/=0;}\nhelper(){value+=3;}', ''
    yield 'multiple', 'value:=17;', 'a(){value+=3;} b(){value*=2;} init(){a();b();a();if(value!=43)value/=0;}', ''
    yield 'empty', 'value:=17;', 'helper(){} init(){helper();if(value!=17)value/=0;}', ''
    yield 'loop', 'value:int;', 'helper(){value++;} init(){for(i:=0;i<4097;i++)helper();if(value!=4097)value/=0;}', ''
    yield 'branches', 'value:=17;', 'helper(){value+=3;} init(){if(value==17)helper();else value/=0;if(value!=20)value/=0;}', ''
    yield 'case', 'key:=17; value:=17;', 'helper(){value+=3;} init(){case key {1=>value/=0; 10 to 20=>helper(); * =>value/=0;} if(value!=20)value/=0;}', ''
    yield 'dense-case', 'key:=1; value:=17;', 'helper(){value+=3;} init(){case key {0=>value/=0; 1=>helper(); 2=>value/=0; * =>value/=0;} if(value!=20)value/=0;}', ''
    yield 'big-case', 'key:=big 16r7fffffffffffffff; value:=17;', 'helper(){value+=3;} init(){case key {big -1=>value/=0; big 16r7fffffffffffffff=>helper(); * =>value/=0;} if(value!=20)value/=0;}', ''
    yield 'case-loop', 'value:int;', 'helper(){value+=3;} init(){for(i:=0;i<4097;i++){case i%3 {0=>helper(); 1=>helper(); 2=>helper(); * =>value/=0;}} if(value!=12291)value/=0;}', ''
    yield 'big-values', 'value:=big 16r7fffffffffffffff;', 'helper(){value+=big 1;} init(){helper();if(value!=big 16r8000000000000000)value/=big 0;}', ''
    yield 'real-move', 'value:real; original:=3.5;', 'helper(){value=original;} init(){helper();if(value!=3.5){a:=17;a/=0;}}', ''
    yield 'caller-locals', 'value:=17;', 'helper(){value+=3;} init(){x:=big 17;y:=big 33;z:=1.25;helper();if(value!=20||x!=big 17||y!=big 33||z!=1.25)value/=0;}', ''
    yield 'scratch', 'a:=12345;b:=789;answer:int;', '\nhelper(){answer=((a^b)&65535)|65536;}\ninit(){x:=big 17;y:=big 33;helper();helper();if(answer!=78636||x!=big 17||y!=big 33)answer/=0;}', ''
    yield 'body-boundary', 'value:int;', 'helper(){'+('value++;'*32)+'} init(){helper();if(value!=32)value/=0;}', ''
    definitions = ' '.join('h'+str(i)+'(){value++;}' for i in range(15))
    calls = ''.join('h'+str(i)+'();' for i in range(15))
    yield 'function-boundary', 'value:int;', definitions+' init(){'+calls+'if(value!=15)value/=0;}', ''
    yield 'local-word', 'value:int;', 'helper(){x:int;x++;value+=x;} init(){helper();helper();if(value!=2)value/=0;}', ''
    yield 'local-loop', 'value:int;', 'helper(){x:int;x++;value+=x;} init(){for(i:=0;i<4097;i++)helper();if(value!=4097)value/=0;}', ''
    yield 'local-big', 'value:big;', 'helper(){x:big;x+=big 1;value+=x;} init(){helper();helper();if(value!=big 2)value/=big 0;}', ''
    yield 'local-real', 'value:real;', 'helper(){x:real;value=x;} init(){value=3.5;helper();if(value!=0.0){i:=1;i/=0;}}', ''
    yield 'local-mixed', 'value:int; wide:big; floating:real;', 'helper(){x:int;y:big;z:real;x++;y+=big 1;value+=x;wide+=y;floating=z;} init(){helper();helper();if(value!=2||wide!=big 2||floating!=0.0)value/=0;}', ''
    yield 'local-initialized', 'value:int;', 'helper(){x:=17;x++;value+=x;} init(){helper();helper();if(value!=36)value/=0;}', ''
    yield 'local-reuse', 'value:int; wide:big;', 'a(){x:big;x=big -1;wide=x;} b(){x:int;x++;value+=x;} init(){a();b();a();b();if(wide!=big -1||value!=2)value/=0;}', ''
    yield 'local-case-loop', 'value:int;', 'helper(){x:int;x++;value+=x;} init(){for(i:=0;i<4097;i++){case i%3 {0=>helper();1=>helper();2=>helper();* =>value/=0;}}if(value!=4097)value/=0;}', ''
    locals_ = ''.join('x'+str(i)+':big;' for i in range(12))
    updates = ''.join('x'+str(i)+'+=big 1;value+=x'+str(i)+';' for i in range(12))
    yield 'local-frame-boundary', 'value:big;', 'helper(){'+locals_+updates+'} init(){helper();helper();if(value!=big 24)value/=big 0;}', ''
    yield 'callee-error', 'value:=17;', '\nhelper(){value/=0;}\ninit(){helper();}', 'zero divide'
    yield 'wrong-answer', 'value:=17;', 'helper(){value+=3;} init(){helper();if(value!=21)value/=0;}', 'zero divide'

    yield 'helper-if', 'value:int;', 'helper(){if(value==0)value=7;else value+=9;} init(){helper();helper();if(value!=16)value/=0;}', ''
    yield 'helper-for', 'value:int;', 'helper(){x:int;for(i:=0;i<9;i++)x+=i;value+=x;} init(){helper();helper();if(value!=72)value/=0;}', ''
    yield 'helper-while', 'value:int;', 'helper(){x:int;while(x<9){x++;value+=x;}} init(){helper();helper();if(value!=90)value/=0;}', ''
    yield 'helper-zero-loop', 'value:int;', 'helper(){x:int;while(x<0)value++;} init(){helper();if(value!=0)value/=0;}', ''
    yield 'helper-nested', 'value:int;', 'helper(){x:int;for(i:=0;i<3;i++)for(j:=0;j<4;j++)x+=i+j;value+=x;} init(){helper();helper();if(value!=60)value/=0;}', ''
    yield 'helper-break', 'value:int;', 'helper(){x:int;for(i:=0;i<9;i++){if(i==3)continue;if(i==7)break;x+=i;}value+=x;} init(){helper();helper();if(value!=36)value/=0;}', ''
    yield 'helper-return', 'key:int;value:int;', 'helper(){if(key==0){value+=7;return;}value+=9;} init(){helper();key=1;helper();if(value!=16)value/=0;}', ''
    yield 'helper-big', 'value:big;', 'helper(){x:big;for(i:=big 0;i<big 9;i++)x+=i;value+=x;} init(){helper();helper();if(value!=big 72)value/=big 0;}', ''
    yield 'helper-real', 'value:int;key:=1.25;', 'helper(){if(key<2.0)value+=3;else value+=7;} init(){helper();key=3.5;helper();if(value!=10)value/=0;}', ''
    yield 'helper-after-entry', 'value:int;', 'init(){helper();helper();if(value!=12)value/=0;} helper(){for(i:=0;i<3;i++)value+=i+1;}', ''
    yield 'helper-multiple', 'value:int;', 'a(){if(value==0)value=1;else value+=2;} helper(){x:int;while(x<3){x++;value+=x;}} init(){a();helper();a();helper();if(value!=15)value/=0;}', ''
    yield 'helper-long-caller', 'value:int;', 'helper(){x:int;for(i:=0;i<4;i++)x+=i;value+=x;} init(){for(i:=0;i<4097;i++)helper();if(value!=24582)value/=0;}', ''
    yield 'helper-loop-error', 'value:int;', 'helper(){for(i:=0;i<4;i++){if(i==2)value/=0;value++;}} init(){helper();}', 'zero divide'
    yield 'helper-return-continue', 'value:int;', 'helper(){if(value==0)return;value++;} init(){helper();value/=0;}', 'zero divide'

    for key, answer in ((-1,40),(0,10),(1,20),(2,30),(9,40)):
        yield 'helper-dense-'+str(key), 'key:='+str(key)+';value:int;', 'helper(){case key {0=>value+=10;1=>value+=20;2=>value+=30;* =>value+=40;}} init(){helper();helper();if(value!='+str(answer*2)+')value/=0;}', ''
    for key, answer in ((-1,3),(15,7),(99,11)):
        yield 'helper-range-'+str(key), 'key:='+str(key)+';value:int;', 'helper(){case key {-1=>value+=3;10 to 20=>value+=7;* =>value+=11;}} init(){helper();helper();if(value!='+str(answer*2)+')value/=0;}', ''
    yield 'helper-case-locals', 'key:int;value:int;', 'helper(){x:int;case key {0=>x++;1=>x+=3;* =>x+=5;}value+=x;} init(){helper();key=1;helper();key=8;helper();if(value!=9)value/=0;}', ''
    yield 'helper-case-return', 'key:int;value:int;', 'helper(){case key {0=>value+=3;return;1=>value+=7;* =>value+=11;}value++;} init(){helper();key=1;helper();key=8;helper();if(value!=23)value/=0;}', ''
    yield 'helper-case-loop', 'value:int;', 'helper(){x:int;for(i:=0;i<6;i++){case i%3 {0=>x+=1;1=>x+=3;2=>x+=5;* =>x/=0;}}value+=x;} init(){helper();helper();if(value!=36)value/=0;}', ''
    yield 'helper-case-big', 'key:=big 16r7fffffffffffffff;value:int;', 'helper(){case key {big -1=>value+=3;big 16r7fffffffffffffff=>value+=7;* =>value+=11;}} init(){helper();key=big -1;helper();key=big 8;helper();if(value!=21)value/=0;}', ''
    yield 'helper-case-after-entry', 'key:=15;value:int;', 'init(){helper();helper();if(value!=14)value/=0;} helper(){case key {-1=>value+=3;10 to 20=>value+=7;* =>value+=11;}}', ''
    yield 'helper-case-multiple', 'key:int;value:int;', 'a(){case key {0=>value+=1;5 to 8=>value+=3;* =>value+=5;}} b(){case key {0=>value+=7;5 to 8=>value+=11;* =>value+=13;}} init(){a();b();key=6;a();b();key=20;a();b();if(value!=40)value/=0;}', ''
    yield 'helper-case-caller', 'value:int;', 'helper(){x:int;case value%3 {0=>x+=1;1=>x+=1;2=>x+=1;* =>x/=0;}value+=x;} init(){for(i:=0;i<4097;i++){case i%2 {0=>helper();1=>helper();* =>value/=0;}}if(value!=4097)value/=0;}', ''

    labels = ''.join(str(i*10)+'=>value+='+str(i+1)+';' for i in range(8))
    yield 'helper-case-boundary', 'key:=70;value:int;', 'helper(){case key {'+labels+'* =>value/=0;}} init(){helper();helper();if(value!=16)value/=0;}', ''
    yield 'helper-case-two-tables', 'key:int;value:int;', 'helper(){case key {0=>value+=1;5 to 8=>value+=3;* =>value+=5;}case key {0=>value+=7;5 to 8=>value+=11;* =>value+=13;}} init(){helper();key=6;helper();if(value!=22)value/=0;}', ''
    yield 'helper-case-nested', 'key:int;other:=6;value:int;', 'helper(){case key {0=>case other {0=>value+=1;5 to 8=>value+=3;* =>value+=5;}10 to 20=>value+=7;* =>value+=9;}} init(){helper();key=15;helper();if(value!=10)value/=0;}', ''
    yield 'helper-case-error', 'key:=15;value:int;', '\nhelper(){case key {-1=>value++;10 to 20=>value/=0;* =>value--;}}\ninit(){helper();}', 'zero divide'

    yield 'helper-nested-call', 'value:int;', 'inner(){value++;} helper(){inner();inner();} init(){helper();helper();if(value!=4)value/=0;}', ''
    yield 'helper-nested-locals', 'value:int;', '\ninner(){x:int;x+=7;value+=x;}\nhelper(){x:int;x=3;inner();if(x!=3)value/=0;value+=x;}\ninit(){helper();helper();if(value!=20)value/=0;}', ''
    yield 'helper-nested-loop', 'value:int;', 'inner(){x:int;x++;value+=x;} helper(){x:int;for(i:=0;i<9;i++){inner();x+=i;}value+=x;} init(){helper();helper();if(value!=90)value/=0;}', ''
    yield 'helper-nested-return', 'key:int;value:int;', 'inner(){if(key==0)return;value+=7;} helper(){x:=3;inner();if(x!=3)value/=0;value+=x;} init(){helper();key=1;helper();if(value!=13)value/=0;}', ''
    yield 'helper-nested-big-real', 'value:big;floating:real;', 'inner(){x:big;y:real;x+=big 7;value+=x;floating=y;} helper(){x:=big 17;y:=1.25;inner();if(x!=big 17||y!=1.25){i:=1;i/=0;}value+=x;} init(){helper();helper();if(value!=big 48||floating!=0.0){i:=1;i/=0;}}', ''
    yield 'helper-nested-case-call', 'key:=15;value:int;', 'inner(){x:int;case key {-1=>x+=3;10 to 20=>x+=7;* =>x+=11;}value+=x;} helper(){x:=3;case key {-1=>inner();10 to 20=>inner();* =>inner();}if(x!=3)value/=0;value+=x;} init(){helper();helper();if(value!=20)value/=0;}', ''
    yield 'helper-nested-shared', 'value:int;', 'leaf(){x:int;x++;value+=x;} a(){x:=3;leaf();value+=x;} b(){x:=7;leaf();value+=x;} helper(){x:=11;a();b();leaf();value+=x;} init(){helper();helper();if(value!=48)value/=0;}', ''
    yield 'helper-nested-after-entry', 'value:int;', 'init(){helper();helper();if(value!=20)value/=0;} helper(){x:=3;inner();value+=x;} inner(){x:int;x+=7;value+=x;}', ''
    yield 'helper-nested-parent-return', 'key:int;value:int;', 'leaf(){value+=7;} middle(){leaf();if(key==0)return;value+=3;} helper(){middle();value+=11;} init(){helper();key=1;helper();if(value!=39)value/=0;}', ''
    yield 'helper-nested-conditional', 'key:int;value:int;', 'a(){value+=3;} b(){value+=7;} helper(){x:=11;if(key==0)a();else b();if(x!=11)value/=0;value+=x;} init(){helper();key=1;helper();if(value!=32)value/=0;}', ''
    yield 'helper-nested-many-callers', 'value:int;', 'leaf(){x:int;x++;value+=x;} middle(){x:=3;leaf();value+=x;} helper(){x:=7;middle();value+=x;} init(){leaf();middle();helper();if(value!=16)value/=0;}', ''
    yield 'helper-nested-empty', 'value:int;', 'empty(){} helper(){x:=3;empty();value+=x;} init(){helper();helper();if(value!=6)value/=0;}', ''
    yield 'helper-nested-fault', 'value:int;', '\ninner(){value/=0;}\nhelper(){inner();}\ninit(){helper();}', 'zero divide'
    definitions = ' '.join('h'+str(i)+'(){x:=1;'+('h'+str(i+1)+'();' if i<14 else '')+'value+=x;}' for i in range(15))
    yield 'helper-nested-depth', 'value:int;', definitions+' init(){h0();h0();if(value!=30)value/=0;}', ''

    yield 'helper-arg-word', 'value:int;', 'helper(x:int){value+=x;} init(){helper(3);helper(7);if(value!=10)value/=0;}', ''
    yield 'helper-arg-big', 'value:big;', 'helper(x:big){value+=x;} init(){helper(big 16r7fffffffffffffff);helper(big 1);if(value!=big 16r8000000000000000)value/=big 0;}', ''
    yield 'helper-arg-real', 'value:real;', 'helper(x:real){value=x;} init(){helper(1.25);if(value!=1.25){i:=1;i/=0;}helper(-3.5);if(value!= -3.5){i:=1;i/=0;}}', ''
    yield 'helper-arg-mixed', 'value:int;', 'helper(a:int,b:big,c:real){if(a==3&&b==big 7&&c==1.25)value++;else value/=0;} init(){helper(3,big 7,1.25);helper(3,big 7,1.25);if(value!=2)value/=0;}', ''
    yield 'helper-arg-locals', 'value:int;', 'helper(x:int){y:int;y++;value+=x+y;} init(){helper(3);helper(7);if(value!=12)value/=0;}', ''
    yield 'helper-arg-mutate', 'value:int;', 'helper(x:int){x++;value+=x;} init(){x:=3;helper(x);helper(x);if(x!=3||value!=8)value/=0;}', ''
    yield 'helper-arg-nested', 'value:int;', 'inner(x:int){y:int;y++;value+=x+y;} helper(x:int){y:=11;inner(x);if(y!=11)value/=0;value+=x;} init(){helper(3);helper(7);if(value!=22)value/=0;}', ''
    yield 'helper-arg-swap', 'value:int;', 'inner(a:int,b:int){value+=a*10+b;} helper(a:int,b:int){inner(b,a);if(a!=3||b!=7)value/=0;} init(){helper(3,7);helper(3,7);if(value!=146)value/=0;}', ''
    yield 'helper-arg-loop', 'value:int;', 'helper(x:int){y:int;y++;value+=x+y;} init(){for(i:=0;i<4097;i++)helper(i);if(value!=8394753)value/=0;}', ''
    yield 'helper-arg-branch', 'value:int;', 'helper(x:int){if(x==0)return;value+=x;} init(){helper(0);helper(7);if(value!=7)value/=0;}', ''
    yield 'helper-arg-case', 'value:int;', 'helper(x:int){case x {-1=>value+=3;10 to 20=>value+=7;* =>value+=11;}} init(){helper(-1);helper(15);helper(99);if(value!=21)value/=0;}', ''
    yield 'helper-arg-after-entry', 'value:int;', 'init(){helper(3);helper(7);if(value!=10)value/=0;} helper(x:int){value+=x;}', ''
    yield 'helper-arg-caller-live', 'value:int;', 'helper(a:big,b:real){x:big;y:real;x=a;y=b;if(x!=big 17||y!=1.25)value/=0;value++;} init(){x:=big 17;y:=1.25;helper(x,y);helper(x,y);if(x!=big 17||y!=1.25||value!=2)value/=0;}', ''
    yield 'helper-arg-case-call', 'value:int;key:=15;', 'inner(x:int){value+=x;} helper(x:int){case key {-1=>inner(x);10 to 20=>inner(x);* =>inner(x);}} init(){helper(3);helper(7);if(value!=10)value/=0;}', ''

    for key,answer in ((0,3),(1,7)):
        yield 'helper-arg-conditional-'+str(key), 'value:int;key:='+str(key)+';', 'helper(x:int){value+=x;} init(){if(key==0)helper(3);else helper(7);if(value!='+str(answer)+')value/=0;}', ''
    for key,answer in ((0,3),(15,7),(99,11)):
        yield 'helper-arg-case-branch-'+str(key), 'value:int;key:='+str(key)+';', 'helper(x:int){value+=x;} init(){case key {0=>helper(3);10 to 20=>helper(7);* =>helper(11);}if(value!='+str(answer)+')value/=0;}', ''
    declarations = ','.join('x'+str(i)+':big' for i in range(12))
    updates = ''.join('value+=x'+str(i)+';' for i in range(12))
    arguments = ','.join('big 1' for i in range(12))
    yield 'helper-arg-frame-boundary', 'value:big;', 'helper('+declarations+'){'+updates+'} init(){helper('+arguments+');helper('+arguments+');if(value!=big 24)value/=big 0;}', ''
    yield 'helper-arg-empty', 'value:int;', 'helper(x:int){} init(){helper(3);helper(7);value++;if(value!=1)value/=0;}', ''
    yield 'helper-arg-padding', 'value:int;', 'helper(a:int,b:big,c:int,d:real){x:int;x++;if(a!=3||b!=big 7||c!=11||d!=1.25)value/=0;value+=x;} init(){helper(3,big 7,11,1.25);helper(3,big 7,11,1.25);if(value!=2)value/=0;}', ''

    for bound in (2147483647,-2147483648,536870912,-536870913):
        literal = HELPERS['literal'](bound)
        yield 'helper-arg-word-bound-'+str(bound), 'value:int;', 'helper(x:int){value=x;} init(){helper('+literal+');if(value!='+literal+')value/=0;}', ''


# These sources must be valid ordinary Limbo before the scalar backend rejects their calling shapes.
def refusals():
    yield 'byte-arguments', 'value:int;', 'helper(x:byte){value+=int x;} init(){helper(byte 3);}'
    yield 'managed-result', 'value:int;', 'helper():string{value++;return "x";} init(){answer:=helper();value+=len answer;}'
    yield 'byte-result', 'value:int;', 'helper():byte{value++;return byte value;} init(){answer:=helper();value+=int answer;}'
    yield 'real-arithmetic-result', 'value:int;x:real;', 'helper():real{value++;return x+1.0;} init(){answer:=helper();if(answer!=1.0)value/=0;}'
    yield 'tuple-managed-result', 'value:int;', 'helper():(int,string){value++;return(value,"x");} init(){(a,b):=helper();value+=a+len b;}'
    yield 'tuple-byte-result', 'value:int;', 'helper():(int,byte){value++;return(value,byte 7);} init(){(a,b):=helper();value+=a+int b;}'
    yield 'tuple-nested-result', 'value:int;', 'helper():((int,int),big){value++;return((value,2),big 3);} init(){(a,b):=helper();(c,d):=a;value+=c+d+int b;}'
    yield 'tuple-argument', 'value:int;', 'helper(a:(int,int)){value++;(x,y):=a;value+=x+y;} init(){helper((1,2));}'
    yield 'tuple-global', 'value:int;state:(int,int);', 'helper():(int,int){value++;return(1,2);} init(){state=helper();(x,y):=state;value+=x+y;}'
    yield 'tuple-field-limit', 'value:int;', 'helper():(int,int,int,int,int,int,int,int,int){value++;return(1,2,3,4,5,6,7,8,9);} init(){helper();}'
    yield 'tuple-partial-unpack', 'calls:int;x:=7;', 'helper(a:int):(int,big,real){calls++;return(a,big 17,1.25);} init(){(u,nil,w):=helper(x);if(u!=7||w!=1.25||calls!=1)calls/=0;}'
    yield 'recursive', 'value:int;', 'helper(){value++;helper();} init(){helper();}'
    yield 'mutual-recursion', 'value:int;', 'inner(){value++;helper();} helper(){inner();} init(){helper();}'
    yield 'byte-local', 'value:int;', 'helper(){x:byte;x=byte value;value=int x;} init(){helper();}'
    yield 'helper-table-limit', 'value:int;', 'helper(){case value {'+''.join(str(i*10)+'=>value+='+str(i+1)+';' for i in range(9))+'* =>value--;}} init(){helper();}'
    yield 'managed', 'value: string;', 'helper(){value="hello";} init(){helper();}'
    locals_ = ''.join('x'+str(i)+':big;' for i in range(13))
    updates = ''.join('x'+str(i)+'+=big 1;value+=x'+str(i)+';' for i in range(13))
    yield 'frame-limit', 'value:big;', 'helper(){'+locals_+updates+'} init(){helper();}'
    yield 'body-limit', 'value:int;', 'helper(){'+('value++;'*33)+'} init(){helper();}'
    body = ' '.join('h'+str(i)+'(){value++;}' for i in range(16))
    calls = ''.join('h'+str(i)+'();' for i in range(16))
    yield 'function-limit', 'value:int;', body+' init(){'+calls+'}'
    yield 'computed-real-argument', 'value:int;x:real;', 'helper(a:real){if(a!=1.0)value++;} init(){helper(x+1.0);}'
    yield 'managed-argument', 'value:int;', 'helper(s:string){value=len s;} init(){helper("x");}'
    yield 'entry-cycle', 'value:int;', 'helper(){init();} init(){helper();}'
    body = ' '.join('h'+str(i)+'(){x:=1;'+('h'+str(i+1)+'();h'+str(i+1)+'();' if i<14 else '')+'value+=x;}' for i in range(15))
    yield 'expansion-limit', 'value:int;', body+' init(){h0();}'


# Derive the caller-local boundary from ordinary assembly, independently of the preparer's frame arithmetic.
def scratch_window(assembly, image, rows, locations, helperline):
    entrytype = int(re.search(r'\tentry\t\d+, (\d+)', assembly)[1])
    legacy = int(re.search(r'\tdesc\t\$'+str(entrytype)+r',(\d+),', assembly)[1])
    nsections = struct.unpack_from('>I', image, 28)[0]
    for i in range(nsections):
        kind, flags, at, size = struct.unpack_from('>HHII', image, 32+12*i)
        if kind == 3:
            at += 4
            data = struct.unpack_from('>I', image, at)[0]
            at += 8+(data+63)//64
            frame = struct.unpack_from('>I', image, at)[0]
            break
    else:
        raise RuntimeError('Missing frame descriptor')
    low, high = ((legacy+7)//8)*8+36, frame-16
    offsets = []
    for row, loc in zip(rows, locations):
        if not loc['start'] or loc['start']['line'] != helperline:
            continue
        mode, args, index = row['mode'], row['args'], 0
        for present, fp in ((bool(mode&0xc0),(mode&0xc0)==0x80),
                            (((mode>>3)&7)!=3,((mode>>3)&7)==1),
                            ((mode&7)!=3,(mode&7)==1)):
            if present:
                if fp:
                    offsets.append(args[index])
                index += 1
    if not offsets or any(value < low or value >= high for value in offsets):
        raise RuntimeError('Callee scratch overlaps caller storage')
    # A current-frame scratch address is valid VM storage but is not a callee-private cell.
    corrupted = offsets+[frame-12]
    if all(low <= value < high for value in corrupted):
        raise RuntimeError('Caller scratch control escaped')


# Replace the second call's zero stores with OR zero: valid scalar code that exposes stale local state.
def stale_locals(image, rows):
    result = bytearray(image)
    sections = struct.unpack_from('>I', image, 28)[0]
    code = next(struct.unpack_from('>HHII', image, 32+12*i)[2] for i in range(sections)
                if struct.unpack_from('>H', image, 32+12*i)[0] == 2)
    names = re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT/'include/isa.h').read_text(), re.M)
    stores, at = [], code+4
    for row in rows:
        if row['op'] == names.index('IMOVW') and row['mode'] == 0x11 and row['args'][0] == 0:
            stores.append(at)
        at += 2
        for unused in row['args']:
            first = image[at]
            at += 1 if first < 128 else 2 if first < 192 else 4
    if len(stores) < 8 or len(stores)%2:
        raise RuntimeError('Missing two complete helper initialization spans')
    for at in stores[len(stores)//2:]:
        result[at] = names.index('IORW')
    return result


# Alias the child's actual local onto the parent's actual local, retaining valid scalar encoding.
def overlap_depth(image, rows, locations, text):
    names = re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT/'include/isa.h').read_text(), re.M)
    parentline = next(i+1 for i,line in enumerate(text.splitlines()) if line.startswith('helper()'))
    childline = next(i+1 for i,line in enumerate(text.splitlines()) if line.startswith('inner()'))
    parent = [row['args'][-1] for row,loc in zip(rows,locations) if loc['start'] and loc['start']['line']==parentline
              and row['op']==names.index('IMOVW') and row['mode']==0x11 and row['args'][0]==3]
    child = [row['args'][-1] for row,loc in zip(rows,locations) if loc['start'] and loc['start']['line']==childline
             and row['op']==names.index('IADDW') and row['mode']==0x11 and row['args'][0]==7]
    if not parent or not child or len(set(parent))!=1 or len(set(child))!=1 or child[0]<=parent[0]:
        raise RuntimeError('Missing distinct active parent/child local witnesses')
    bias = child[0]-parent[0]
    result = bytearray(image)
    sections = struct.unpack_from('>I', image, 28)[0]
    at = next(struct.unpack_from('>HHII', image, 32+12*i)[2] for i in range(sections)
              if struct.unpack_from('>H', image, 32+12*i)[0]==2)+4
    changed = 0
    for row,loc in zip(rows,locations):
        at += 2
        mode, index = row['mode'], 0
        for present,fp in ((bool(mode&0xc0),(mode&0xc0)==0x80),
                           (((mode>>3)&7)!=3,((mode>>3)&7)==1),((mode&7)!=3,(mode&7)==1)):
            if not present:
                continue
            first = image[at]
            size = 1 if first<128 else 2 if first<192 else 4
            if fp and loc['start'] and loc['start']['line']==childline:
                value = row['args'][index]-bias
                if -64<=value<64:
                    encoded = bytes([value&127])
                elif -8192<=value<8192:
                    encoded = bytes([0x80|((value>>8)&63),value&255])
                else:
                    encoded = bytes([0xc0|((value>>24)&63),(value>>16)&255,(value>>8)&255,value&255])
                if len(encoded)!=size:
                    raise RuntimeError('Depth control must preserve instruction framing')
                result[at:at+size] = encoded
                changed += 1
            at += size
            index += 1
    if not changed:
        raise RuntimeError('Inactive parent/child overlap control')
    return result


# Preserve framing while withholding a specific nonzero argument from the child's initialized cells.
def zero_argument(image, rows):
    result = bytearray(image)
    sections = struct.unpack_from('>I', image, 28)[0]
    at = next(struct.unpack_from('>HHII', image, 32+12*i)[2] for i in range(sections)
              if struct.unpack_from('>H', image, 32+12*i)[0]==2)+4
    names = re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT/'include/isa.h').read_text(), re.M)
    changed = 0
    for row in rows:
        at += 2
        if row['op']==names.index('IMOVW') and row['mode']==0x11 and row['args'][0]==3:
            if image[at]!=3:
                raise RuntimeError('Missing canonical argument value')
            result[at] = 0
            changed += 1
        for unused in row['args']:
            first = image[at]
            at += 1 if first<128 else 2 if first<192 else 4
    if changed!=1:
        raise RuntimeError('Missing unique argument copy witness')
    return result


# Change only the actual returned arithmetic value, preserving valid scalar operands and framing.
def wrong_result(image, rows, locations, text):
    names = re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT/'include/isa.h').read_text(), re.M)
    line = next(i+1 for i,value in enumerate(text.splitlines()) if value == 'return x+calls;')
    result = bytearray(image)
    sections = struct.unpack_from('>I', image, 28)[0]
    at = next(struct.unpack_from('>HHII', image, 32+12*i)[2] for i in range(sections)
              if struct.unpack_from('>H', image, 32+12*i)[0]==2)+4
    changed = 0
    for row,loc in zip(rows,locations):
        if loc['start'] and loc['start']['line']==line and row['op']==names.index('IADDW'):
            result[at] = names.index('ISUBW')
            changed += 1
        at += 2
        for unused in row['args']:
            first = image[at]
            at += 1 if first<128 else 2 if first<192 else 4
    if changed!=1:
        raise RuntimeError('Missing unique result producer')
    return result


# Alias the second actual Word result onto the first while retaining valid reader-admitted storage.
def overlap_tuple(image, rows, locations, text):
    names = re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT/'include/isa.h').read_text(), re.M)
    line = next(i+1 for i,value in enumerate(text.splitlines()) if value == 'return (x+1,x+2);')
    stores = [row['args'][-1] for row,loc in zip(rows,locations)
              if loc['start'] and loc['start']['line']==line and row['op']==names.index('IADDW')]
    if len(stores)!=2 or stores[1]!=stores[0]+4:
        raise RuntimeError('Missing distinct tuple result fields')
    result = bytearray(image)
    sections = struct.unpack_from('>I', image, 28)[0]
    at = next(struct.unpack_from('>HHII', image, 32+12*i)[2] for i in range(sections)
              if struct.unpack_from('>H', image, 32+12*i)[0]==2)+4
    changed = 0
    for row,loc in zip(rows,locations):
        at += 2
        for index,value in enumerate(row['args']):
            first = image[at]
            size = 1 if first<128 else 2 if first<192 else 4
            if loc['start'] and loc['start']['line']==line and row['op']==names.index('IADDW') and index==len(row['args'])-1 and value==stores[1]:
                value = stores[0]
                if value<64:
                    encoded = bytes([value])
                elif value<8192:
                    encoded = bytes([0x80|((value>>8)&63),value&255])
                else:
                    encoded = bytes([0xc0|((value>>24)&63),(value>>16)&255,(value>>8)&255,value&255])
                if len(encoded)!=size:
                    raise RuntimeError('Tuple control changed operand framing')
                result[at:at+size] = encoded
                changed += 1
            at += size
    if changed!=1:
        raise RuntimeError('Inactive tuple overlap control')
    return result


# Compare real compiler products, preserve diagnostics, and execute whole OS processes with explicit target runners.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--emu', type=Path)
    parser.add_argument('--runner', default='')
    parser.add_argument('--reader', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    command = HELPERS['command']
    count = processes = controls = depthcontrols = argcontrols = resultcontrols = tuplecontrols = refused = 0
    with tempfile.TemporaryDirectory(prefix='inline scalar ', dir=root) as directory:
        work = Path(directory)
        source, image, smap, assembly, legacy = (work/name for name in ('source.b','image.dis64','map.json','source.s','legacy.dis'))
        for name, globals_, body, error in programs():
            # A correct helper return must reach a distinct caller-only fault.
            if name.startswith('helper-') and not error:
                begin = body.index('{', body.index('init()'))
                depth = 1
                end = begin+1
                while depth:
                    depth += (body[end] == '{')-(body[end] == '}')
                    end += 1
                body = body[:end-1]+'continuation_probe <<=32;'+body[end-1:]
                globals_ += ' continuation_probe:int;'
                error = 'invalid argument'
            text = PREFIX+globals_+'\n'+body+'\n'
            source.write_text(text)
            command([args.limbo.resolve(), '-S', '-o', assembly, source])
            if name.startswith(('helper-case', 'helper-dense', 'helper-range')):
                tables = re.findall(r'^\t(?:case|casel|goto)\t', assembly.read_text(), re.M)
                minimum = 2 if name in ('helper-case-two-tables', 'helper-case-nested', 'helper-case-multiple') else 1
                if len(tables) < minimum:
                    raise RuntimeError('Missing real helper case tables: '+name)
            if '\tframe\t' not in assembly.read_text() or '\tcall\t' not in assembly.read_text():
                raise RuntimeError('Missing real source call: '+name)
            command([args.limbo.resolve(), '-o', legacy, source])
            before = legacy.read_bytes()
            command([sys.executable, ROOT/'utils/dis64-locations.py', 'compile', '--limbo', args.limbo.resolve(),
                     '--source', source, '--image', image, '--map', smap])
            rows = HELPERS['decoded'](image.read_bytes())
            locations = json.loads(smap.read_bytes())['instructions']
            if len(locations) != len(rows) or any(row['pc'] != pc or row['op'] != rows[pc]['op'] for pc,row in enumerate(locations)):
                raise RuntimeError('Prepared source-map identity mismatch')
            if name == 'local-frame-boundary' and not re.search(r'\tdesc\t\$\d+,128,', assembly.read_text()):
                raise RuntimeError('Missing actual 128-byte helper frame boundary')
            if name == 'scratch':
                helperline = next(i+1 for i,line in enumerate(text.splitlines()) if line.startswith('helper()'))
                scratch_window(assembly.read_text(), image.read_bytes(), rows, locations, helperline)
            if args.reader:
                command([args.reader.resolve(), image, '--pilot'])
            command([args.limbo.resolve(), '-o', legacy, source])
            if before != legacy.read_bytes():
                raise RuntimeError('Specialization changed ordinary Dis')
            if args.emu:
                for jit in (0,1):
                    output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                        '-c', str(jit), '-d', '/'+str(image.relative_to(root))])
                    marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="'+error+'"'
                    if marker not in output or ('Broken:' in output) != bool(error) or 'native64 scalar scheduler empty after program retirement' not in output or 'panic:' in output or 'faults:' in output:
                        raise RuntimeError(name+' JIT='+str(jit)+'\n'+output)
                    if name.startswith('helper-tuple-fault-') or name in ('callee-error', 'helper-case-error', 'helper-nested-fault', 'helper-result-zero-divide', 'helper-result-invalid-shift'):
                        faults = re.findall(r'native64 scalar fault: dis-pc=(\d+)',output)
                        if name.startswith('helper-tuple-fault-'):
                            line = next(i+1 for i,s in enumerate(text.splitlines()) if s.startswith('return ('))
                        elif name.startswith('helper-result-'):
                            line = next(i+1 for i,s in enumerate(text.splitlines()) if s.startswith('return a'))
                        else:
                            line = next(i+1 for i,s in enumerate(text.splitlines()) if s.startswith('inner()' if name == 'helper-nested-fault' else 'helper()'))
                        if len(faults)!=1 or locations[int(faults[0])]['start']['line']!=line:
                            raise RuntimeError('Inlined error lost helper source identity')
                    processes += 1
            if name == 'helper-nested-locals':
                corrupted = work/'overlap-depth.dis64'
                corrupted.write_bytes(overlap_depth(image.read_bytes(), rows, locations, text))
                if args.reader:
                    command([args.reader.resolve(), corrupted, '--pilot'])
                if args.emu:
                    for jit in (0,1):
                        output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                            '-c', str(jit), '-d', '/'+str(corrupted.relative_to(root))])
                        marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="zero divide"'
                        if marker not in output or 'native64 scalar scheduler empty after program retirement' not in output or 'panic:' in output:
                            raise RuntimeError('Parent/child overlap control escaped\n'+output)
                        depthcontrols += 1
            if name == 'helper-tuple-control':
                corrupted = work/'overlap-tuple.dis64'
                corrupted.write_bytes(overlap_tuple(image.read_bytes(), rows, locations, text))
                if args.reader:
                    command([args.reader.resolve(), corrupted, '--pilot'])
                if args.emu:
                    for jit in (0,1):
                        output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                            '-c', str(jit), '-d', '/'+str(corrupted.relative_to(root))])
                        marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="zero divide"'
                        if marker not in output or 'native64 scalar scheduler empty after program retirement' not in output or 'panic:' in output:
                            raise RuntimeError('Tuple result overlap control escaped\n'+output)
                        tuplecontrols += 1
            if name == 'helper-result-control':
                corrupted = work/'wrong-result.dis64'
                corrupted.write_bytes(wrong_result(image.read_bytes(), rows, locations, text))
                if args.reader:
                    command([args.reader.resolve(), corrupted, '--pilot'])
                if args.emu:
                    for jit in (0,1):
                        output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                            '-c', str(jit), '-d', '/'+str(corrupted.relative_to(root))])
                        marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="zero divide"'
                        if marker not in output or 'native64 scalar scheduler empty after program retirement' not in output or 'panic:' in output:
                            raise RuntimeError('Returned value control escaped\n'+output)
                        resultcontrols += 1
            if name == 'helper-arg-locals':
                corrupted = work/'zero-argument.dis64'
                corrupted.write_bytes(zero_argument(image.read_bytes(), rows))
                if args.reader:
                    command([args.reader.resolve(), corrupted, '--pilot'])
                if args.emu:
                    for jit in (0,1):
                        output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                            '-c', str(jit), '-d', '/'+str(corrupted.relative_to(root))])
                        marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="zero divide"'
                        if marker not in output or 'native64 scalar scheduler empty after program retirement' not in output or 'panic:' in output:
                            raise RuntimeError('Argument initialization control escaped\n'+output)
                        argcontrols += 1
            if name == 'local-word':
                corrupted = work/'stale-locals.dis64'
                corrupted.write_bytes(stale_locals(image.read_bytes(), rows))
                if args.reader:
                    command([args.reader.resolve(), corrupted, '--pilot'])
                if args.emu:
                    for jit in (0,1):
                        output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                            '-c', str(jit), '-d', '/'+str(corrupted.relative_to(root))])
                        marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="zero divide"'
                        if marker not in output or 'native64 scalar scheduler empty after program retirement' not in output:
                            raise RuntimeError('Stale helper locals control escaped\n'+output)
                        controls += 1
            count += 1
        for name,globals_,body in refusals():
            refused += 1
            source.write_text(PREFIX+globals_+'\n'+body+'\n')
            command([args.limbo.resolve(), '-o', legacy, source])
            result = subprocess.run([str(args.limbo.resolve()), '-6', '-o', str(image), str(source)],
                                    capture_output=True, timeout=30)
            diagnostic = result.stdout+result.stderr
            if result.returncode <= 0 or b'Dis64 scalar:' not in diagnostic or b'runtime error:' in diagnostic:
                raise RuntimeError('Missing controlled scalar refusal: '+name+'\n'+diagnostic.decode(errors='replace'))
            if image.exists():
                raise RuntimeError('Refused helper left an executable image: '+name)
    print('PASS scalar helpers:',count,'sources,',processes,'whole C/JIT processes;',controls,'active stale-local controls;',depthcontrols,'active depth-overlap controls;',argcontrols,'active argument controls;',resultcontrols,'active result controls;',tuplecontrols,'active tuple controls; ordinary Dis unchanged;',refused,'calling/storage limits refused')


if __name__ == '__main__':
    main()
