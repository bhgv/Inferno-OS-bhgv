"""Mapped String/ListString values around file calls; no managed wire kind IDs."""
from pathlib import Path
import runpy
ROOT=Path(__file__).resolve().parents[2];BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-library-files.py'))
def cases(pathlength=13):
 provider=BASE['source']('inc:fn(n:int):int;','inc(n:int):int{return n+1;}')
 rows=[
 ('string-length','s:="library.dis64";n:=len s;',pathlength,'keeps:=s;',0),
 ('string-nil','s:string;n:=len s;',0,'keeps:=s;',0),
 ('string-alias','s:="library.dis64";alias:=s;s=nil;n:=len alias;',pathlength,'keeps:=s;keepalias:=alias;',0),
 ('list-empty','l:list of string;n:=len l;',0,'keepl:=l;',0),
 ('list-cons-head-tail','s:="library.dis64";l:=s::nil;h:=hd l;t:=tl l;n:=len l+len h+len t;',pathlength+1,'keeps:=s;keepl:=l;keeph:=h;keept:=t;',0),
 ('list-shared-tail','s:="library.dis64";l:=s::nil;alias:=l;l=s::l;l=nil;h:=hd alias;n:=len alias+len h;',pathlength+1,'keeps:=s;keepl:=l;keepalias:=alias;keeph:=h;',0),
 ('list-nil-string','s:string;l:=s::nil;h:=hd l;n:=len l+len h;',1,'keeps:=s;keepl:=l;keeph:=h;',0),
 ('list-loop','s:="library.dis64";l:list of string;i:=0;while(i<4){l=s::l;i++;}n:=len l;',4,'keeps:=s;keepl:=l;',0),
 ('list-walk','s:="library.dis64";l:=s::s::nil;total:=0;while(l!=nil){h:=hd l;total+=len h;l=tl l;}n:=total;',2*pathlength,'keeps:=s;keepl:=l;',0),
 ('list-nil-branch','s:="library.dis64";l:list of string;if(l==nil)l=s::nil;else l=s::s::nil;n:=len l;',1,'keeps:=s;keepl:=l;',0),
 ('list-head-nil','l:list of string;s:=hd l;n:=len s;',0,'keepl:=l;keeps:=s;',3),
 ('list-tail-nil','l:list of string;t:=tl l;n:=len t;',0,'keepl:=l;keept:=t;',3)]
 for name,body,expected,keep,failure in rows:
  text='implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";if(m==nil)return;first:=m->inc(7);'+body+'last:=m->inc(n);if(last!='+str(expected+1)+'){zero:=0;bad:=1/zero;}'+keep+'}'
  yield(name,provider,'inc',1,[1],[7],expected+1,failure,text,True)
