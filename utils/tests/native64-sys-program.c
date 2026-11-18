/* Real source recipes now execute FRAME, MOVP/clear, owned LEA binding and
 * MCALL into the explicit trampoline. Source snapshots/OS transports remain
 * fixture inputs; the complete emuinit functions are not admitted here. */
static Module *programowner;
static void programbusy(void){check(programowner!=nil && !Sys_clearframes64(programowner));}
static int programcallprobe(void *frame){Sys_programcall64(frame);return 1;}
static int programexec(void *unused)
{
 VmFetch fetched;unsigned int steps=0;USED(unused);
 while(vmfetch(&fetched) && fetched.op!=IEXIT){
  check(++steps<32);
  if(R.M->m->cstep && R.M->m->cstep())continue;
  vmdecode(&fetched);
  switch(fetched.op){case IFRAME:frame();break;case IMOVP:movp();break;case IMCALL:mcall();break;default:check(0);}
  process.R=R;epochs(6);
 }
 return 1;
}

static void vmprogramplans(void)
{
 const Native64GeneratedStore *programs[]={emuinit_call_0,emuinit_call_1,emuinit_call_2};
 const Native64GeneratedBank *banks[]={&emuinit_call_0_bank,&emuinit_call_1_bank,&emuinit_call_2_bank};
 const uchar *maps[]={emuinit_call_0_framemap,emuinit_call_1_framemap,emuinit_call_2_framemap};
 const unsigned int sizes[]={emuinit_call_0_framebytes,emuinit_call_1_framebytes,emuinit_call_2_framebytes};
 const char *expected[]={"emuinit: cannot load /dis/lib/arg.dis: typed error\n","typed error","emuinit: unable to load chosen: failure\n"};
 Native64SysProgramSpec spec,bad[2];Native64TailSlot slots[2];Native64SysSites *installed;
 const Native64GeneratedBank *bank;const Native64GeneratedCell *cell;const Native64GeneratedStore *store;
 Type *ct,*ft,*nt,*fdt,*types[2];Module target={0};Modlink *native;Frame *caller;
 Inst code[16],snapshot[16];uchar map[16];void *value,*result;String *text;Sys_FD *fd;WORD status;
 unsigned int b,phase,i,j,pc,nargs,count,resultoffset,linkoffset,niloffset,refs[3],before;
 REG saved=R,active;Frame *entered;Modlink foreign;Inst *returnpc;int diagnosticcalls;

 for(phase=0;phase<3;phase++)for(b=0;b<3;b++){
  contextstart(phase,Pready);sourcebankpool=1;process.pid=99;
  writemode=0;writefprint=b!=1;releasedvm=releases=acquires=writes=writeresolves=movedword=fdretired=0;nativecount=-99;
  bank=banks[b];resultoffset=bank->bytes;linkoffset=resultoffset+8;niloffset=linkoffset+8;
  memset(map,0,sizeof(map));memmove(map,bank->map,bank->mapbytes);
  map[linkoffset/64]|=128>>((linkoffset/8)%8);map[niloffset/64]|=128>>((niloffset/8)%8);
  if(b==1)map[resultoffset/64]|=128>>((resultoffset/8)%8);
  ct=dtype(freeheap,niloffset+8,map,(niloffset+71)/64);check(ct!=nil);
  ft=dtype(freeheap,sizes[b],(uchar*)maps[b],2);check(ft!=nil);
  nt=dtype(freeheap,sizeof(Modlink),nil,0);check(nt!=nil);
  native=H2D(Modlink*,heapz(nt));native->m=&target;native->MP=H;native->prog=nil;native->nlinks=1;
  native->links[0].frame=ft;native->links[0].u.runt=Sys_programcall64;
  caller=(Frame*)stackframes(ct,1);R=process.R;R.FP=(uchar*)caller;R.TS=stackspace.bytes+sizeof(stackspace.bytes);process.R=R;
  R.s=&native;R.d=R.FP+linkoffset;movp();destroy(native);
  types[0]=ct;types[1]=ft;programowner=rootlink->m;programowner->ref=1;programowner->type=types;programowner->ntype=2;programowner->origmp=H;programowner->cstep=native64typedlea8step;
  fdt=TFD=b==1?nil:dtype(freeheap,sizeof(Sys_FD),nil,0);if(b!=1)check(fdt!=nil);
  nargs=b==0?3:b==1?1:4;count=b==0?1:b==1?0:2;
  for(i=0;i<bank->count;i++){
   cell=bank->cells+i;store=nil;
   for(j=0;j<nargs;j++)if(programs[b][j].cell==(int)i){store=programs[b]+j;break;}check(store && cell->root && !cell->setupop);
   if(b!=1 && store->fixed && store->ordinal==0){fd=H2D(Sys_FD*,heapz(fdt));fd->fd=7;value=fd;}
   else{text=cell->seed==1?c2string((char*)cell->initial,cell->length):c2string(store->ordinal==0?"chosen":"failure",store->ordinal==0?6:7);value=text;}
   R.s=&value;R.d=R.FP+64+8*i;movp();destroy(value);process.R=R;epochs(6);
  }
  memset(code,0,sizeof(code));pc=0;
  code[pc].op=IFRAME;code[pc].add=SRC(AIMM)|DST(AFP);code[pc].s.imm=1;code[pc++].d.ind=40;
  for(i=0;i<nargs;i++){
   store=programs[b]+i;code[pc].op=IMOVP;code[pc].add=SRC(AFP)|DST(AFP|AIND);code[pc].s.ind=store->source_offset;code[pc].d.i.f=40;code[pc++].d.i.s=store->destination;
   if(b!=1 && i==0){code[pc].op=IMOVP;code[pc].add=SRC(AFP)|DST(AFP);code[pc].s.ind=niloffset;code[pc++].d.ind=store->source_offset;}
  }
  memset(&spec,0,sizeof(spec));spec.bindpc=pc;spec.callpc=pc+1;
  code[pc].op=ILEA;code[pc].add=SRC(AFP)|DST(AFP|AIND);code[pc].reg=b==1?24:4;code[pc].s.ind=resultoffset;code[pc].d.i.f=40;code[pc++].d.i.s=32;
  code[pc].op=IMCALL;code[pc].add=SRC(AFP)|AXIMM|DST(AFP);code[pc].s.ind=40;code[pc++].d.ind=linkoffset;
  code[pc].op=IEXIT;code[pc++].add=SRC(AXXX)|DST(AXXX);rootlink->prog=programowner->prog=code;programowner->nprog=pc;rootlink->type=types;
  spec.frame=(Native64SysFrameSpec){b==1?Native64SysSprint:Native64SysFprint,ft,ct,resultoffset,slots,count,8,128};
  for(i=0;i<count;i++)slots[i]=(Native64TailSlot){Native64TailRef,i*8,8,&Tstring};
  memmove(snapshot,code,sizeof(code));refs[0]=ct->ref;refs[1]=ft->ref;refs[2]=Tstring.ref;
  for(i=0;i<7;i++){
   bad[0]=spec;bad[1]=spec;
   if(i==0)bad[0].bindpc=pc;
   if(i==1)bad[0].callpc++;
   if(i==2)bad[0].frame.result_offset+=4;
   if(i==3)code[spec.bindpc].reg^=8;
   if(i==4)code[spec.callpc].s.ind=48;
   if(i==5)code[spec.callpc].d.ind=0;
   before=contextattempt;check(!Sys_setprogram64(programowner,bad,i==6?2:1));
   check(contextattempt==before && !programowner->native64sysframes && !programowner->native64sysstep && ct->ref==refs[0] && ft->ref==refs[1] && Tstring.ref==refs[2]);
   memmove(code,snapshot,sizeof(code));
  }
  for(i=1;i<=1+(count!=0);i++){
   contextfailure=contextattempt+i;check(!Sys_setprogram64(programowner,&spec,1));contextfailure=0;
   check(!programowner->native64sysframes && !programowner->native64sysstep && ct->ref==refs[0] && ft->ref==refs[1] && Tstring.ref==refs[2]);
  }
  check(Sys_setprogram64(programowner,&spec,1));installed=programowner->native64sysframes;
  check(installed->owner==programowner && ct->ref==refs[0]+1 && ft->ref==refs[1]+1 && Tstring.ref==refs[2]+count);
  memset(&spec,0xa5,sizeof(spec));memset(slots,0xa5,sizeof(slots));
  R.PC=code;R.M=rootlink;R.MP=H;process.R=R;nativewriteobserver=programbusy;
  check(kprotect(programexec,nil));nativewriteobserver=nil;
  check(R.M==rootlink && R.FP==(uchar*)caller && R.SP==(uchar*)caller+ct->size);
  check(!memcmp(code,snapshot,sizeof(code)) && installed->busy==0);
  if(b==1){memmove(&text,R.FP+resultoffset,8);check(text!=H && text->len==(int)strlen(expected[b]) && !memcmp(text->Sascii,expected[b],text->len));}
  else{memmove(&status,R.FP+resultoffset,4);check(status==(int)strlen(expected[b]) && nativecount==status && !memcmp(nativebytes,expected[b],status));
   check(writes==1 && releases==1 && acquires==1);memmove(&value,R.FP+programs[b][0].source_offset,8);check(value==H);}
  /* Recreate a readable native activation in the real current extent. All
   * payload roots are H; malformed identities must fail before allocation. */
  active=R;entered=(Frame*)((uchar*)caller+ct->size);initmem(ft,entered);
  memmove(&native,(uchar*)caller+linkoffset,8);foreign=*rootlink;returnpc=entered->lr;
  diagnosticcalls=writes;
  for(i=0;i<6;i++){
   entered->lr=returnpc;entered->fp=(uchar*)caller;entered->mr=rootlink;entered->t=ft;
   R=active;R.FP=(uchar*)entered;R.SP=(uchar*)entered+ft->size;R.M=native;
   if(i==0)entered->lr=installed->sites[0].bindpc;
   if(i==1){foreign.m=&target;entered->mr=&foreign;}
   if(i==2)native->links[0].frame=ct;
   if(i==3)native->links[0].u.runt=Roots_bridge;
   if(i==4)installed->sites[0].callpc->d.ind^=8;
   if(i==5)installed->owner=&target;
   process.R=R;before=contextattempt;
   check(!kprotect(programcallprobe,entered));
   check(contextattempt==before && writes==diagnosticcalls && !strcmp(up->env->errstr,exBounds) && !installed->busy);
   installed->owner=programowner;native->links[0].frame=ft;native->links[0].u.runt=Sys_programcall64;
   memmove(code,snapshot,sizeof(code));
  }
  R=active;process.R=R;
  check(Sys_clearframes64(programowner) && !programowner->native64sysstep);
  check(ct->ref==refs[0] && ft->ref==refs[1]);
  freeptrs(caller,ct);initmem(ct,caller);process.R.EX=process.R.SP=process.R.FP=nil;
  rootlink->prog=nil;programowner->prog=nil;programowner->nprog=0;programowner->type=nil;programowner->ntype=0;programowner->cstep=nil;rootlink->type=nil;
  freetype(nt);freetype(ft);freetype(ct);if(fdt)freetype(fdt);TFD=nil;clearrootcache();epochs(6);
  check(contextlive==0 && sourcebanklive==0 && Tstring.ref==1);sourcebankpool=0;
 }
 programowner=nil;R=saved;process.R=R;
 puts("PASS owned Sys program bridge: nine real emuinit recipe FRAME/MOVP/LEA/MCALL executions, copied site/Type owners, seven metadata and six live identity refusals, allocation rollback, busy output and complete retirement; full functions not executed");
}
