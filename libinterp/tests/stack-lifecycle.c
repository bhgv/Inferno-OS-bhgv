#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <stdio.h>
#include <setjmp.h>


/* Keep inherited fixture checks active in release-style sanitizer builds. */
static void
lifecyclecondition(int condition)
{
	if(!condition)abort();
}
#define lifecyclecheck(condition) lifecyclecondition(!!(condition)) // Normalize pointer/scalar predicates without assert.

REG R;
#define T(r) *((void**)R.r) // Native decoded pointer slot used by borrowed stack helpers.
char exBounds[]="bounds"; // Decoded source slot bounds error.
char exHeap[]="heap",exNomem[]="memory",exModule[]="module",exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
static int failure,allocations,releases,checks;
static void *owned;
static ulong ownedsize;


/* Observe the actual VM error identity at an independent fixture boundary. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}
/* Keep active address/stack checking without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
/* Stack payload maps contain language nil references in this fixture. */
void
destroy(void *p)
{
	lifecyclecheck(p==H);
}


/* Record the requested extent and poison it before production header setup. */
static void*
allocate(ulong n,int clear)
{
	void *p;

	allocations++;
	if(failure)return nil;
	lifecyclecheck(owned==nil && n<4096);
	p=malloc(n+32);lifecyclecheck(p);
	memset(p,clear ? 0 : 0xa5,n);memset((uchar*)p+n,0x5a,32);
	owned=p;ownedsize=n;
	return p;
}


/* Verify both the extent owner and its guard on the actual free path. */
static void
releaseextent(void *p)
{
	int i;

	lifecyclecheck(p==owned);
	for(i=0;i<32;i++)lifecyclecheck(((uchar*)p)[ownedsize+i]==0x5a);
	free(p);owned=nil;releases++;
}

#define mallocz(n,c) allocate((n),(c))
#define free(p) releaseextent(p)
IMPLEMENTATION
#undef free
#undef mallocz


/* Construct a frame without introducing alignment assumptions in the fixture. */
static void
putframe(uchar *p,Type *t)
{
	static Frame frame; // Stable fixture image across native error unwinds.
	void *nilowner=H;

	memset(&frame,0,sizeof(frame));

	frame.t=t;
	memmove(p,&frame,sizeof(frame));
	memmove(p+sizeof(frame),&nilowner,sizeof(nilowner));
}


/* Check copied frame bytes and transactional publication around allocation failure. */
static void
copyframes(void)
{
	union {uvlong align;uchar bytes[256];} frame,slot;
	uchar saved[256],expected[128];
	Type type={0};Module module={0};Modlink link={0};
	Prog child={0};REG before;
	union {uvlong align;uchar bytes[256];} rootbuf;
	Stkext *root=(Stkext*)rootbuf.bytes,*extent;
	Frame *source;
	ulong hints[]={0,64,256};
	int a,b,kind,h,fail;

	type.size=sizeof(Frame)+32;module.ss=0;link.m=&module;
	for(a=0;a<8;a++)for(b=0;b<8;b++)for(kind=0;kind<2;kind++)
	for(h=0;h<nelem(hints);h++)for(fail=0;fail<2;fail++){
		memset(frame.bytes,0x67,sizeof(frame.bytes));
		putframe(frame.bytes+16+a,&type);
		memset(rootbuf.bytes,0x67,sizeof(rootbuf.bytes));
		root->reg.TR=&type;putframe(root->reg.tos.fu,nil);
		source=kind ? root->reg.tos.fr : (Frame*)(frame.bytes+16+a);
		memmove(slot.bytes+16+b,&source,sizeof(source));
		memmove(saved,frame.bytes,sizeof(saved));
		memset(&R,0,sizeof(R));R.s=slot.bytes+16+b;
		memset(&child,0,sizeof(child));child.R.M=&link;child.R.SP=(uchar*)1;
		module.ss=hints[h];before=child.R;
		allocations=releases=0;failure=fail;raised=nil;
		if(setjmp(boundary)==0){newstack(&child);lifecyclecheck(!fail);}
		else lifecyclecheck(fail && raised==exNomem);
		lifecyclecheck(allocations==1 && !memcmp(saved,frame.bytes,sizeof(saved)));
		if(fail)lifecyclecheck(!memcmp(&before,&child.R,sizeof(before)) && owned==nil);
		else{
			extent=owned;
			lifecyclecheck(child.R.EX==(uchar*)extent && child.R.FP==extent->reg.tos.fu);
			lifecyclecheck(child.R.SP==child.R.FP+type.size && child.R.TS==(uchar*)extent+ownedsize);
			lifecyclecheck(extent->reg.TR==&type && extent->reg.EX==nil && extent->reg.SP==nil);
			memmove(expected,source,type.size);memset(expected,0,sizeof(Frame));
			lifecyclecheck(!memcmp(expected,child.R.FP,type.size));releaseextent(owned);
		}
		checks++;
	}
}


/* Unframe may release a source extension or retain an ordinary source frame. */
static void
popframes(void)
{
	union {uvlong align;uchar bytes[256];} current,sourcebuf,slot,currentroot;
	Type type={0};Frame *source;
	Stkext *extent;
	REG expected;
	int a,b,c,kind,root;

	type.size=sizeof(Frame)+32;
	for(a=0;a<8;a++)for(b=0;b<8;b++)for(c=0;c<8;c++)for(kind=0;kind<2;kind++)for(root=0;root<2;root++){
		memset(&R,0,sizeof(R));putframe(current.bytes+16+a,&type);
		R.FP=current.bytes+16+a;
		if(root){
			Stkext *sx=(Stkext*)currentroot.bytes;
			sx->reg.TR=&type;putframe(sx->reg.tos.fu,nil);R.FP=sx->reg.tos.fu;
		}
		R.SP=(uchar*)1;R.TS=(uchar*)2;R.EX=(uchar*)3;
		allocations=releases=0;failure=0;
		if(kind){
			extent=allocate(sizeof(Stkext)+64,1);
			extent->reg.TR=&type;extent->reg.TS=(uchar*)4;extent->reg.EX=(uchar*)5;
			putframe(extent->reg.tos.fu,nil);source=extent->reg.tos.fr;
		}else{
			putframe(sourcebuf.bytes+16+c,&type);source=(Frame*)(sourcebuf.bytes+16+c);
		}
		memmove(slot.bytes+16+b,&source,sizeof(source));R.s=slot.bytes+16+b;
		expected=R;expected.SP=R.FP+type.size;
		if(kind){expected.TS=(uchar*)4;expected.EX=(uchar*)5;}
		unframe();lifecyclecheck(!memcmp(&R,&expected,sizeof(R)));
		lifecyclecheck(releases==kind && owned==nil);checks++;
	}
}


/* Round-trip real extent initialization and mapped-pointer cleanup. */
static void
extentcycle(void)
{
	Type type={0};Module module={0};Modlink link={0};
	REG before;
	Stkext *extent;
	Frame *frame;
	void *payload;
	ulong hints[]={0,128,1024};
	int h,mapped,fail;

	type.size=sizeof(Frame)+32;link.m=&module;
	for(h=0;h<nelem(hints);h++)for(mapped=0;mapped<2;mapped++)for(fail=0;fail<2;fail++){
		type.np=mapped;type.map[0]=1<<(7-sizeof(Frame)/sizeof(void*));
		module.ss=hints[h];
		memset(&R,0,sizeof(R));R.s=&type;R.M=&link;
		R.SP=(uchar*)1;R.TS=(uchar*)2;R.EX=(uchar*)3;
		before=R;failure=fail;allocations=releases=0;raised=nil;
		if(setjmp(boundary)==0){extend();lifecyclecheck(!fail);}
		else lifecyclecheck(fail && raised==exNomem);
		lifecyclecheck(allocations==1);
		if(fail){lifecyclecheck(!memcmp(&before,&R,sizeof(R)) && owned==nil);}
		else{
			extent=owned;frame=R.s;
			lifecyclecheck(frame==extent->reg.tos.fr && frame->t==nil && frame->mr==nil);
			lifecyclecheck(R.SP==(uchar*)frame+type.size && R.TS==(uchar*)extent+ownedsize);
			if(mapped){memmove(&payload,(uchar*)frame+sizeof(Frame),sizeof(payload));lifecyclecheck(payload==H);}
			unextend(frame);
			lifecyclecheck(R.SP==before.SP && R.TS==before.TS && R.EX==before.EX && R.M==before.M);
			lifecyclecheck(releases==1 && owned==nil);
		}
		checks++;
	}
}


/* Invalid owners/descriptors must fail before allocation or register publication. */
static void
invalid(void)
{
	union {uvlong align;uchar bytes[256];} frame,slot,badsource;
	Type type={0};Module module={0};Modlink link={0};
	Prog child;REG before;
	Frame *source;
	int op,which,align;
	char *wanted;

	for(op=0;op<3;op++)for(which=0;which<13;which++)for(align=0;align<8;align++){
		type.size=sizeof(Frame)+32;module.ss=0;link.m=&module;
		putframe(frame.bytes+16+align,&type);source=(Frame*)(frame.bytes+16+align);
		memset(&R,0,sizeof(R));memset(&child,0,sizeof(child));
		R.M=child.R.M=&link;R.FP=frame.bytes+16+align;R.SP=(uchar*)1;
		wanted=exHeap;
		if(which<2){source=which ? H : nil;wanted=exNilref;}
		if(which==2)type.size=-1;
		if(which==3)type.size=sizeof(Frame)-1;
		if(which==4){Type *bad=H;memmove(frame.bytes+16+align+O(Frame,t),&bad,sizeof(bad));}
		if(which==5)type.size=0x7fffffff;
		if(which==6){R.M=child.R.M=nil;wanted=exModule;}
		if(which==7){R.M=child.R.M=H;wanted=exModule;}
		if(which==8){link.m=nil;wanted=exModule;}
		if(which==9){link.m=H;wanted=exModule;}
		if(which==10){putframe(badsource.bytes+16+align,H);source=(Frame*)(badsource.bytes+16+align);}
		if(which==11){R.FP=nil;wanted=exNilref;}
		if(which==12){R.FP=H;wanted=exNilref;}
		if(op<2 && which>=10)continue;
		if(op==1 && which<2)continue;
		if(op==2 && which>=5 && which<10)continue;
		memmove(slot.bytes+16+align,&source,sizeof(source));R.s=slot.bytes+16+align;
		if(op==1)R.s=which==4 ? H : &type;
		before=op==0 ? child.R : R;
		allocations=releases=0;failure=0;raised=nil;
		if(setjmp(boundary)==0){
			if(op==0)newstack(&child);
			if(op==1)extend();
			if(op==2)unframe();
			lifecyclecheck(0);
		}
		lifecyclecheck(raised==wanted && allocations==0 && releases==0 && owned==nil);
		lifecyclecheck(!memcmp(&before,op==0 ? &child.R : &R,sizeof(before)));checks++;
	}
}


/* Reject negative pointer-map lengths before allocation or stack publication. */
static void
negativemaps(void)
{
	union {uvlong align;uchar bytes[256];} storage;
	Type type={0};
	Module module={0};
	Modlink link={0};
	Prog child;
	Frame *source;
	REG before;
	int op;

	type.size=sizeof(Frame)+32;type.np=-1;link.m=&module;
	for(op=0;op<3;op++){
		memset(&R,0,sizeof(R));memset(&child,0,sizeof(child));
		putframe(storage.bytes,&type);source=(Frame*)storage.bytes;
		R.M=child.R.M=&link;R.FP=storage.bytes;
		R.s=op==1 ? (void*)&type : (void*)&source;
		before=op==0 ? child.R : R;
		allocations=releases=0;failure=0;raised=nil;
		if(setjmp(boundary)==0){
			if(op==0)newstack(&child);
			if(op==1)extend();
			if(op==2)unframe();
			abort();
		}
		if(raised!=exHeap || allocations || releases || owned ||
		   memcmp(&before,op==0 ? &child.R : &R,sizeof(before)))abort();
		checks++;
	}
}


/* Invalid extension owners must leave saved registers and storage untouched. */
static void
invalidunextend(void)
{
	union {uvlong align;uchar bytes[256];} storage;
	Stkext *extent=(Stkext*)storage.bytes;
	Type type={0};
	Type *selected;
	Frame *frame;
	REG before;
	int mode;
	char *wanted;

	for(mode=0;mode<6;mode++){
		memset(storage.bytes,0xa5,sizeof(storage.bytes));memset(&R,0,sizeof(R));
		type.np=mode==5 ? -1 : 0;selected=&type;
		if(mode==2)selected=nil;
		if(mode==3)selected=H;
		if(mode==4)type.np=(-0x7fffffff-1);
		extent->reg.TR=selected;frame=extent->reg.tos.fr;
		if(mode==0)frame=nil;
		if(mode==1)frame=H;
		wanted=mode<2 ? exNilref : exHeap;
		R.SP=(uchar*)1;R.TS=(uchar*)2;R.EX=(uchar*)3;before=R;
		allocations=releases=0;failure=0;raised=nil;
		if(setjmp(boundary)==0){unextend(frame);abort();}
		if(raised!=wanted || allocations || releases || owned || memcmp(&before,&R,sizeof(R)))abort();
		checks++;
	}
}


/* Keep error-bearing families independent under address/undefined sanitizers. */
int
main(int argc,char **argv)
{
	lifecyclecheck(argc==2);
	if(!strcmp(argv[1],"copy"))copyframes();
	if(!strcmp(argv[1],"pop"))popframes();
	if(!strcmp(argv[1],"extent"))extentcycle();
	if(!strcmp(argv[1],"invalid")){invalid();negativemaps();invalidunextend();}
	lifecyclecheck(checks>0 && owned==nil);
	printf("PASS stack lifecycle: %d %s checks WORD%zu\n",checks,argv[1],sizeof(WORD)*8);
	return 0;
}
