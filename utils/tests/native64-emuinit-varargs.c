/* Replay real source argument plans through typed gateways. The scheduler/file
 * transports remain the existing formatter fixture; this is not a whole body. */
#include "emuinit-varargs-native64.h"
static void vmemuinitplans(void)
{
 const Native64GeneratedStore *programs[]={emuinit_call_0,emuinit_call_1,emuinit_call_2};
 const Native64GeneratedBank *banks[]={&emuinit_call_0_bank,&emuinit_call_1_bank,&emuinit_call_2_bank};
 const uchar *maps[]={emuinit_call_0_framemap,emuinit_call_1_framemap,emuinit_call_2_framemap};
 const unsigned int sizes[]={emuinit_call_0_framebytes,emuinit_call_1_framebytes,emuinit_call_2_framebytes};
 const unsigned int clearcells[]={emuinit_call_0_clearcell,~0U,emuinit_call_2_clearcell};
 const char *expected[]={"emuinit: cannot load /dis/lib/arg.dis: typed error\n","typed error","emuinit: unable to load chosen: failure\n"};
 const Native64GeneratedBank *bank;const Native64GeneratedCell *cell;const Native64GeneratedStore *store;
 Native64Result owner={H,0};Native64SysWriteRequest request;Native64TailSlot slots[2];SysFormatTest result;
 Type *type,*caller_type,*fdtype;String *text;Sys_FD *fd;uchar map[SlotBytes/64+1],*callee;
 unsigned int b,phase,i,j,total,bases[2],at,nargs,ntail;void *origin,*value;WORD count;REG saved=R;
 for(phase=0;phase<3;phase++)for(b=0;b<3;b++){
  contextstart(phase,Pready);sourcebankpool=1;formatscenario=0;
  writemode=0;writefprint=b!=1;releasedvm=releases=acquires=writes=writeresolves=movedword=fdretired=0;nativecount=-99;
  caller_type=native64frametype(&limbo_native64_Roots_caller_entry);check(caller_type!=nil);
  writecaller=(F_Roots_caller*)stackframes(caller_type,1);writecaller->padding=77;
  memset(&result,0,sizeof(result));result.caller=writecaller;
  bank=banks[b];memset(map,0,sizeof(map));total=bank->bytes;
  memmove(map,bank->map,bank->mapbytes);
  for(i=0;i<2;i++){
   bases[i]=total;check(total%8==0);
   for(j=0;j<bank->sources[i].bytes/8;j++)if(bank->sources[i].map[j/8]&(128>>(j%8))){at=total/8+j;map[at/8]|=128>>(at%8);}
   total+=bank->sources[i].bytes;
  }
  at=total;total+=sizes[b];check(total<=SlotBytes);
  for(j=0;j<sizes[b]/8;j++)if(maps[b][j/8]&(128>>(j%8)))map[(at/8+j)/8]|=128>>((at/8+j)%8);
  type=dtype(freeheap,total,map,(total+63)/64);check(type!=nil);
  check(native64resultadopt(&owner,H2D(void*,heapz(type))));callee=(uchar*)owner.value+at;
  fdtype=nil;TFD=nil;
  if(b!=1){fdtype=TFD=dtype(freeheap,sizeof(Sys_FD),nil,0);check(fdtype!=nil);}
  nargs=b==0?3:b==1?1:4;ntail=b==0?1:b==1?0:2;
  for(i=0;i<bank->count;i++){
   cell=bank->cells+i;check(cell->root && !cell->setupop);
   origin=(uchar*)owner.value+bases[cell->space]+cell->offset;
   store=nil;for(j=0;j<nargs;j++)if(programs[b][j].cell==(int)i){store=programs[b]+j;break;}check(store!=nil);
   if(b!=1 && store->fixed && store->ordinal==0){fd=H2D(Sys_FD*,heapz(fdtype));fd->fd=7;value=fd;}
   else{
    check(cell->seed==1 || !cell->seed);
    text=cell->seed==1?c2string((char*)cell->initial,cell->length):c2string(store->ordinal==0?"chosen":"failure",store->ordinal==0?6:7);value=text;
   }
   R.s=&value;R.d=origin;movp();destroy(value);epochs(6);
  }
  for(i=0;i<nargs;i++){
   store=programs[b]+i;cell=bank->cells+store->cell;check(store->op==IMOVP && !store->immediate);
   origin=(uchar*)owner.value+bases[cell->space]+cell->offset;
   R.s=origin;R.d=(uchar*)owner.value+store->source_offset;movp();epochs(6);
   R.s=(uchar*)owner.value+store->source_offset;R.d=callee+store->destination;movp();epochs(6);
   if(b!=1 && i==0){
    check((unsigned int)store->cell==clearcells[b]);value=H;R.s=&value;R.d=origin;movp();
    R.d=(uchar*)owner.value+store->source_offset;movp();epochs(6);
    memmove(&value,callee+64,8);check(value!=H && D2H(value)->ref==1);
   }
  }
  memset(&request,0,sizeof(request));request.format.prog=&process;
  memmove(&request.format.pattern,callee+(b==1?64:72),8);
  request.format.count=ntail;request.format.slots=slots;request.format.values=(uvlong*)(callee+(b==1?72:80));request.format.value_bytes=ntail*8;
  for(i=0;i<ntail;i++)slots[i]=(Native64TailSlot){Native64TailRef,i*8,8,&Tstring};
  request.format.initial=8;request.format.limit=128;
  if(b==1){request.format.resolve=sysformatresolve;request.format.argument=&result;check(Sys_sprint64(&request.format));
   text=(String*)writecaller->result;check(text!=H && text->len==(int)strlen(expected[b]) && !memcmp(text->Sascii,expected[b],text->len));
  }else{request.format.resolve=writecell;memmove(&request.fd,callee+64,8);check(Sys_fprint64(&request));
   memmove(&count,&writecaller->padding,4);check(count==(int)strlen(expected[b]) && nativecount==count && !memcmp(nativebytes,expected[b],count));
   check(writes==1 && releases==1 && acquires==1 && !releasedvm);
  }
  native64resultclear(&owner);epochs(6);freetype(type);
  freeptrs(writecaller,caller_type);initmem(caller_type,writecaller);process.R.EX=process.R.SP=process.R.FP=nil;
  freetype(caller_type);if(fdtype)freetype(fdtype);TFD=nil;clearrootcache();epochs(6);
  check(contextlive==0 && sourcebanklive==0 && Tstring.ref==1);sourcebankpool=0;
 }
 R=saved;process.R=R;
 puts("PASS real emuinit variadic recipes: three exact source plans, nine executions across GC phases, FD owner transfer/clear, actual sprint/fprint gateways and complete retirement; whole emuinit not executed");
}
