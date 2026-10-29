#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include <interp.h>
#include "io.h"
#include "../port/error.h"
#include <isa.h>
#include "kernel.h"
#include "vmfetch.h"
#include "mathi.h"

/* Builtin module support */
#include "bench.h"
#include "benchmod.h"

typedef enum { None, Calibrate, Base, Op, Intr, Dis, Gc, MS2T, xTest};
static struct {
	int		inuse;	/* reference count */
	int		test;
	void*	scratch;
	char*	buf;
	int		bufsz;
	char*	wpos;
	void		(*op)(void);
	vlong	tickstart;
} bench;

static void
log(char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	bench.wpos = vseprint(bench.wpos, bench.buf+bench.bufsz, msg, ap);
	va_end(ap);
}

void
elog(char *msg, ...)
{
	va_list ap;

	if(bench.buf == 0)
		return;
	va_start(ap, msg);
	bench.wpos = vseprint(bench.wpos, bench.buf+bench.bufsz, msg, ap);
	va_end(ap);
}

static void
clear(void)
{
	bench.wpos = bench.buf;
}


/* Validate the logical read extent before forming a pointer into the report buffer. */
static long
rep(void *to, long n, ulong offset)
{
	uintptr used, left;

	if(n <= 0 || bench.buf == nil || bench.wpos == nil ||
	   (uintptr)bench.wpos < (uintptr)bench.buf)
		return 0;
	used = (uintptr)bench.wpos-(uintptr)bench.buf;
	if(bench.bufsz <= 0 || used > (uintptr)bench.bufsz || offset >= used)
		return 0;
	left = used-offset;
	if((uintptr)n > left)
		n = left;
	memmove(to, bench.buf+offset, n);
	return n;
}


static long
notest(int report, void *va, long n, ulong offset)
{
	USED(report, va, n, offset);
	if(report)
		return rep(va, n, offset);
	return 0;
}

/* Calibration is ticks per millisecond, independent of the host long width. */
static vlong MS2TS; // Positive calibrated timer frequency; zero means unavailable.


/* Scale ticks exactly without overflowing a product or touching the VM floating environment. */
static vlong
tickmicro(uvlong ticks)
{
	uvlong frequency, whole, remainder, fraction, residue, result;
	int bit;

	if(MS2TS <= 0)
		return 0;
	frequency = MS2TS;
	whole = ticks/frequency;
	if(whole > 0x7fffffffffffffffULL/1000)
		return 0x7fffffffffffffffLL;
	remainder = ticks%frequency;
	if(remainder <= ~0ULL/1000)
		fraction = remainder*1000/frequency;
	else{
		/* Each residue is below frequency <= INT64_MAX, so both additions fit. */
		fraction = residue = 0;
		for(bit = 512; bit != 0; bit >>= 1){
			fraction *= 2;
			residue *= 2;
			if(residue >= frequency){
				residue -= frequency;
				fraction++;
			}
			if(1000&bit){
				residue += remainder;
				if(residue >= frequency){
					residue -= frequency;
					fraction++;
				}
			}
		}
	}
	result = whole*1000;
	if(fraction > 0x7fffffffffffffffULL-result)
		return 0x7fffffffffffffffLL;
	return result+fraction;
}


/* Subtract timer bit patterns without signed overflow; discard backward/stale samples. */
static vlong
tickspan(vlong end, vlong start)
{
	uvlong span;

	span = (uvlong)end-(uvlong)start;
	if(span > 0x7fffffffffffffffULL)
		return 0;
	return span;
}


/* Bound a measured quantity before conversion, including values rounded up to 2^63. */
static vlong
tickint(double value)
{
	if(!(value > 0.0))
		return 0;
	if(value >= 9223372036854775808.0)
		return 0x7fffffffffffffffLL;
	return (vlong)value;
}


/* Accumulate low 32-bit hardware intervals in a wide total before averaging. */
static long
cal(int report, void *va, long n, ulong offset)
{
	uvlong tot;
	u32int t, low, max;
	int i, lim, pl, mdelay;

	if(report)
		return rep(va, n, offset);
	clear();
	setpri(PriRealtime);
	lim = 1000;
	low = ~0U;
	max = 0;
	tot = 0;
	mdelay = 1000;
	for(i = 0; i < lim; i++){
		pl = splhi();
		t = (u32int)archrdtsc32();
		microdelay(mdelay);
		t = (u32int)archrdtsc32()-t;
		splx(pl);
		if(t < low)
			low = t;
		if(t > max)
			max = t;
		tot += t;
	}
	MS2TS = tot/lim;
	if(va)
		log("mdelay=%d lim=%d tot=%llud low=%ud max=%ud\n", mdelay, lim, tot, low, max);
	setpri(PriNormal);
	return n;
}


/* Format calibrated ticks without integer threshold overflow or division by zero. */
char*
ts2str(vlong ticks)
{
	enum { Nbuf = 5 }; // Retain all fields of one timing report until it is printed.
	static char b[Nbuf][40]; // Rotating result storage used by the report formatter.
	static int next; // Next result slot.
	char *unit;
	double value, magnitude;
	char *out;

	out = b[next];
	next = (next+1)%Nbuf;
	if(MS2TS <= 0){
		snprint(out, sizeof(b[0]), "%lld ticks", ticks);
		return out;
	}
	value = (double)ticks/(double)MS2TS;
	magnitude = value < 0.0 ? -value : value;
	if(magnitude >= 1000.0){
		unit = "s";
		value /= 1000.0;
	}else if(magnitude >= 1.0)
		unit = "ms";
	else if(magnitude >= 0.001){
		unit = "us";
		value *= 1000.0;
	}else{
		unit = "ns";
		value *= 1000000.0;
	}
	snprint(out, sizeof(b[0]), "%.2f %s", value, unit);
	return out;
}


/* Uncalibrated measurements have no time-unit conversion. */
static double
ts2us(vlong ticks)
{
	if(MS2TS <= 0)
		return 0.0;
	return ((double)ticks/(double)MS2TS)*1000.0;
}


/* Reset the origin even before calibration, then return bounded elapsed microseconds. */
static vlong
bus(int reset)
{
	vlong now;

	now = archrdtsc();
	if(reset){
		bench.tickstart = now;
		return 0;
	}
	return tickmicro(tickspan(now, bench.tickstart));
}


/* Measure the timer-read overhead with a wide accumulator and explicit low-word wrap. */
static long
base(int report, void *va, long n, ulong offset)
{
	uvlong tot;
	u32int t, low, max;
	int i, lim, pl;
	char *bm;

	if(report)
		return rep(va, n, offset);
	clear();
	setpri(PriRealtime);
	lim = 1000;
	low = ~0U;
	max = 0;
	tot = 0;
	for(i = 0; i < lim; i++){
		pl = splhi();
		t = (u32int)archrdtsc32();
		t = (u32int)archrdtsc32()-t;
		splx(pl);
		if(t < low)
			low = t;
		if(t > max)
			max = t;
		tot += t;
	}
	bm = ts2str(tot/lim);
	log("%d %d %llud %ud %ud (%s)\n", up->pid, lim, tot, low, max, bm);
	setpri(PriNormal);
	return n;
}

// Timeop

typedef struct Psync Psync;

enum {
	Maxprocs=3,
};

struct Psync {
	Rendez	r;
	int	flag;
	int	id;
	int	awaken;
};
static Psync timesync[Maxprocs];
static Ref nactive;
static Ref nbusy;
static RWlock sync;

static void
nilop(void)
{
}

static int
timev(void *a)
{
	return *(int*)a;
}


/* Measure every selected operation once, keeping the total wider than a low-word sample. */
static void
timeop0(void *ap)
{
	uvlong tot;
	u32int t, low, max;
	int i, lim;
	Psync *ps;
	char *bm;

	ps = ap;
	setpri(PriRealtime);
	incref(&nactive);
	sleep(&ps->r, timev, &ps->flag);
	rlock(&sync);
	lim = 1000;
	low = ~0U;
	max = 0;
	tot = 0;
	for(i = 0; i < lim; i++){
		t = (u32int)archrdtsc32();
		(*bench.op)();
		t = (u32int)archrdtsc32()-t;
		if(t < low)
			low = t;
		if(t > max)
			max = t;
		tot += t;
	}
	bm = ts2str(tot/lim);
	log("%d %d %llud %ud %ud (%s)\n", up->pid, lim, tot, low, max, bm);
	runlock(&sync);
	pexit("", 0);
}

static long
timeop(int report, void *va, long n, ulong offset)
{
	int i, np, pl;

	if(report)
		return rep(va, n, offset);
	clear();
	bench.op = 0;
	if(strncmp(va, "nil", 3) == 0)
		bench.op = nilop;
	else if(strncmp(va, "sched", 5) == 0)
		bench.op = sched;
	else
		return 0;
	for(np=1; np<=Maxprocs; np++) {
		nactive.ref = 0;
		wlock(&sync);
		log("%d procs\n", np);
		setpri(PriRealtime);
		for(i=0; i<np; i++) {
			timesync[i].id = i;
			kproc("timeop", timeop0, &timesync[i], 0);
		}
		while(nactive.ref < np)
			tsleep(&up->sleep, return0, 0, 20);
		for(i=0; i<np; i++){
			timesync[i].flag = 1;
			wakeup(&timesync[i].r);
		}
		sched();
		pl = splhi();
		setpri(PriNormal);
		wunlock(&sync);
		// now they run
		wlock(&sync);		// wait for last reader
		wunlock(&sync);
		splx(pl);
	}
	return n;
}

typedef struct Ictr Ictr;
struct Ictr {
	ulong	base;
	ulong	sleep;
	ulong	spllo;
	ulong	intr;
	ulong	isave;
	ulong	arrive;
	ulong	wakeup;
	ulong	awake;
};
static Ictr counters[5/*100*/], *curct;
static int intrwant;
static Rendez vous;
int	spltbl;	/* set by spllo */
int	intrtbl;	/* set by intrvec() */
int	isavetbl;	/* set by intrvec() */

static int ienable;


static void
intrwake(void)
{
	if(ienable == 0)
		return;
	ienable = 0;
	if(spltbl == 0)		// not used here
		curct->spllo = curct->intr = curct->isave = archrdtsc32();
	else {
		curct->spllo = spltbl;
		curct->intr = intrtbl;
		curct->isave = isavetbl;
	}
	curct->arrive = archrdtsc32();
	intrwant = 0;
	wakeup(&vous);
	curct->wakeup = archrdtsc32();
}

/*
 * sleep calls intrtest with splhi (under lock):
 * provoke the interrupt now, so that it is guaranteed
 * not to happen until sleep has queued the process,
 * forcing wakeup to do something.
 */
static int
intrtest(void*)
{
	ienable = 1;		/* enable recording on interrupt */
	curct->sleep = archrdtsc32();
	return intrwant==0;
}

static long
intrtime(int report, void *va, long n, ulong offset)
{
	Ictr *ic;
	long t;
	int i;
	char *bm;
	if(report)
		return rep(va, n, offset);
	clear();

	setpri(PriRealtime);
	sched();
	curct = counters;
	ienable = 0;
	addclock0link(intrwake, MS2HZ);
	for(i=0; i<nelem(counters); i++){
		curct = &counters[i];
		intrwant = 1;
		curct->base = archrdtsc32();
		sleep(&vous, intrtest, nil);
		curct->awake = archrdtsc32();
		sched();	/* just to slow it down between trials */
	}
	log("interrupt\n");
	for(i=0; i<nelem(counters); i++){
		ic = &counters[i];
		t = ic->awake - ic->base;
		bm = ts2str(ic->awake - ic->arrive);
		ic->awake -= ic->wakeup;
		ic->wakeup -= ic->arrive;
		ic->arrive -= ic->isave;
		ic->isave -= ic->intr;
		ic->intr -= ic->spllo;
		ic->spllo -= ic->sleep;
		ic->sleep -= ic->base;
		log("%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld (%s)\n", ic->sleep, ic->spllo, ic->intr, ic->isave, ic->arrive, ic->wakeup, ic->awake, t, bm);
	}
	setpri(PriNormal);
	return n;
}


/* DIS operation timing */

typedef struct {
	vlong n; // Number of accepted samples, bounded before increment or merge.
	vlong min, max; // Exact minimum and maximum observed tick counts.
	vlong origin; // Exact first sample; centering retains small spreads near huge timestamps.
	double mean; // Mean offset from origin, avoiding overflowing integer sums.
	double m2; // Sum of squared deviations from the mean, not raw squared samples.
} Stat;

enum
{
	Reset, // Clear accumulated timing values.
	Inc, // Include one completed timing sample.
};


/* Form a signed difference as a magnitude before floating conversion, including opposite signs. */
static double
statoffset(vlong value, vlong origin)
{
	if(value >= origin)
		return (double)((uvlong)value-(uvlong)origin);
	return -(double)((uvlong)origin-(uvlong)value);
}


/* Welford accumulation avoids signed squares and cancellation in near-constant samples. */
static void
stat(int op, Stat *c, vlong val)
{
	double delta, value;
	ulong control, status;

	if(op == Reset){
		memset(c, 0, sizeof(*c));
		return;
	}
	if(op != Inc || c->n == 0x7fffffffffffffffLL)
		return;
	if(c->n == 0){
		c->n = 1;
		c->min = c->max = c->origin = val;
		c->mean = c->m2 = 0.0;
		return;
	}
	/* Sampling must not change the measured VM's rounding, traps or sticky flags. */
	control = FPcontrol(RND_NR, RND_MASK|INVAL|ZDIV|OVFL|UNFL|INEX);
	status = FPstatus(0, INVAL|ZDIV|OVFL|UNFL|INEX);
	value = statoffset(val, c->origin);
	delta = value-c->mean;
	c->n++;
	c->mean += delta/(double)c->n;
	c->m2 += delta*(value-c->mean);
	FPstatus(status, INVAL|ZDIV|OVFL|UNFL|INEX);
	FPcontrol(control, RND_MASK|INVAL|ZDIV|OVFL|UNFL|INEX);
	if(val < c->min)
		c->min = val;
	if(val > c->max)
		c->max = val;
}


/* Merge independent centered populations without overflowing count or integer sums. */
static void
statinc(Stat *d, Stat *s)
{
	Stat source;
	vlong count;
	double delta, fraction;

	source = *s;
	if(source.n <= 0 || d->n > 0x7fffffffffffffffLL-source.n)
		return;
	if(d->n == 0){
		*d = source;
		return;
	}
	count = d->n+source.n;
	delta = statoffset(source.origin, d->origin)+source.mean-d->mean;
	fraction = (double)source.n/(double)count;
	d->m2 += source.m2+delta*delta*((double)d->n*fraction);
	d->mean += delta*fraction;
	d->n = count;
	if(source.min < d->min)
		d->min = source.min;
	if(source.max > d->max)
		d->max = source.max;
}


/* Bound conversion at the signed limit where double rounds INT64_MAX up to 2^63. */
static vlong
statmean(Stat *s)
{
	double value;

	if(s->n <= 0)
		return 0;
	value = (double)s->origin+s->mean;
	if(value >= 9223372036854775808.0)
		return s->max;
	if(value <= -9223372036854775808.0)
		return s->min;
	return (vlong)value;
}


enum
{
	HSIZE	= 31,
	MAXCOUNT	= 100000000L,
};

typedef struct {
	int		op;
	int		pc;
	long		count;
	Stat	t;		/* combined dec and execution time */
} Istat;

typedef struct Mstat Mstat;
struct Mstat {
	char*	name;
	char*	path;
	int		ninst;
	Istat*	inst;
	Inst*		base;
	Mstat*	hash;
	Mstat*	link;
};

struct
{
	Mstat *hash[HSIZE]; // Private timing records indexed by copied module names.
	Mstat *list; // All owned records, released by mreset.
	uvlong epoch; // Reset generation checked after callbacks that can release the VM.
} vmstat;

extern struct			/* see ../../interp/tab.h:/keywds/ */
{
	char*	name;
	int	op;
	int	terminal;
}keywds[];

/* Resolve opcode names by their explicit table key, including gaps in ISA slots. */
static char*
opname(int op)
{
	int i;

	if(op < 0 || op >= MAXDIS)
		return "Unknown";
	for(i = 0; keywds[i].name != nil; i++)
		if(keywds[i].op == op)
			return keywds[i].name;
	return "<Noname>";
}


/* Invalidate pending timing pointers before releasing every owned record/string. */
static void
mreset(void)
{
	Mstat *m, *next;

	vmstat.epoch++;
	m = vmstat.list;
	vmstat.list = nil;
	memset(vmstat.hash, 0, sizeof(vmstat.hash));
	for(; m != nil; m = next){
		next = m->link;
		free(m->name);
		free(m->path);
		free(m->inst);
		free(m);
	}
}


static ulong
hash(void *s)
{
	ulong sum = 0;
	uchar *a = s;

	while(*a)
		sum = (sum << 1) + *a++;
	return sum%HSIZE;
}

/* Allocate complete timing storage before publishing a module record. */
static Mstat*
mlookup(Module *mod)
{
	Mstat *m;
	ulong h;

	for(m = vmstat.hash[hash(mod->name)]; m != nil; m = m->hash)
		if(strcmp(m->name, mod->name) == 0 && strcmp(m->path, mod->path) == 0)
			return m;
	if(mod->nprog <= 0 || (ulong)mod->nprog > 0x7fffffff/sizeof(Istat))
		return nil;
	m = malloc(sizeof(*m));
	if(m == nil)
		return nil;
	kstrdup(&m->name, mod->name);
	kstrdup(&m->path, mod->path);
	m->ninst = mod->nprog;
	m->inst = malloc((ulong)m->ninst*sizeof(*m->inst));
	if(m->name == nil || m->path == nil || m->inst == nil){
		free(m->name);
		free(m->path);
		free(m->inst);
		free(m);
		return nil;
	}
	m->base = mod->prog;
	m->link = vmstat.list;
	vmstat.list = m;
	h = hash(m->name);
	m->hash = vmstat.hash[h];
	vmstat.hash[h] = m;
	return m;
}


/* Time checked C steps and discard samples whose record list was reset by C. */
void
bxec(Prog *p)
{
	VmFetch fetched;
	vlong t0, t;
	Mstat *ms;
	Istat *is;
	Prog *child;
	uvlong epoch;
	int pc;

	vmstart(p);
	for(;;){
		if(!vmfetch(&fetched)){
			vmnative();
			break;
		}
		ms = mlookup(fetched.module);
		pc = fetched.index;
		epoch = vmstat.epoch;
		t0 = archrdtsc();
		vmdecode(&fetched);
		vmrun(&fetched);
		t = archrdtsc();
		if(epoch == vmstat.epoch && ms != nil && ms->inst != nil && pc < ms->ninst){
			is = &ms->inst[pc];
			if(is->count < MAXCOUNT){
				if(is->count++ == 0){
					is->op = fetched.op;
					is->pc = pc;
				}
				stat(Inc, &is->t, tickspan(t, t0));
			}
		}
		if(fetched.op == ISPAWN || fetched.op == IMSPAWN){
			child = delruntail(Pdebug);
			if(child == nil || child == H)
				error(exSpawn);
			child->xec = bxec;
			addrun(child);
		}
		if(!vmyield())
			break;
	}
	p->R = R;
}

/* compiled code Dis timing */

static struct { // Pending compiled-code timing sample; private to this device.
	int set; // A pre-hook has opened a timing interval.
	int op, pc; // Dis opcode and validated original instruction index.
	vlong t0, t; // Start/end timestamps for the current sample.
	vlong base; // Measured NOP calibration cost.
	Mstat *ms; // Borrowed record, valid only while epoch matches vmstat.
	Module *om; // Source identity retained for diagnostics, never dereferenced after C.
	uvlong epoch; // Reset generation when the sample's record was acquired.
	int timing; // Timer start/stop selection.
} C;

enum { Nop = 0 }; // Synthetic native NOP used for timing calibration.


/* Close only samples whose record and instruction row survived intervening callbacks. */
void
dopostcomp(vlong t)
{
	Istat *is;

	C.t = t;
	C.set = 0;
	if(C.epoch != vmstat.epoch){
		C.ms = nil;
		C.om = nil;
		return;
	}
	if(C.ms == nil || C.ms->inst == nil || C.pc < 0 || C.pc >= C.ms->ninst)
		return;
	is = &C.ms->inst[C.pc];
	if(C.op == Nop){
		vlong base;

		base = tickspan(C.t, C.t0);
		if(C.base == 0 || base < C.base)
			C.base = base;
	}
	if(is->count < MAXCOUNT){
		if(is->count++ == 0){
			is->op = C.op;
			is->pc = C.pc;
		}
		stat(Inc, &is->t, tickspan(C.t, C.t0));
	}
}


/* A stopped or unopened timer has no pending sample to publish. */
void
postcomp(void)
{
	vlong t;

	t = archrdtsc();
	if(C.timing == 0 || C.set == 0)
		return;
	dopostcomp(t);
}


/* Capture legacy native-ulong hook cells without alignment or stale-owner assumptions. */
void
precomp(void)
{
	Module *module;
	ulong pc, op;
	vlong t;
	int count;

	t = archrdtsc();
	if(C.timing == 0)
		return;
	if(C.set)
		dopostcomp(t);
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(pc) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(op))
		error(exBounds);
	memmove(&pc, R.m, sizeof(pc));
	memmove(&op, R.s, sizeof(op));
	if(pc > 0x7fffffffUL)
		error(exBounds);
	if(op >= MAXDIS)
		error(exOp);
	if(R.M == nil || R.M == H ||
	   (uintptr)R.M > (uintptr)-1-(O(Modlink, m)+sizeof(module)))
		error(exModule);
	memmove(&module, (uchar*)R.M+O(Modlink, m), sizeof(module));
	if(module == nil || module == H || (uintptr)module > (uintptr)-1-sizeof(*module))
		error(exModule);
	memmove(&count, (uchar*)module+O(Module, nprog), sizeof(count));
	if(count <= 0 || pc >= (ulong)count)
		error(exBounds);
	C.pc = pc;
	C.op = op;
	C.om = module;
	C.ms = mlookup(module);
	C.epoch = vmstat.epoch;
	C.set = 1;
	C.t0 = archrdtsc();
}


/* Population deviation is zero for empty or constant data; bound the final conversion. */
static vlong
sdev(Stat *s)
{
	extern double sqrt(double);

	if(s->n <= 0 || !(s->m2 > 0.0))
		return 0;
	return tickint(sqrt(s->m2/(double)s->n));
}


/*
 * Use the sequence:
 * 1. "timer startclr" or "timer start", then,
 * 2. Any DIS operations, and,
 * 3. "timer stop", to stop timing.
 * 4. Read the results from the data file after:
 *	a) "timer report" to get module/pc level results, or
 *	b) "timer summary" to get opcode level results
 */
static long
distime(int report, void *va, long n, ulong offset)
{
	Prog *p;
	Mstat *mp;
	Istat *ip, *ep;

	if(report)
		return rep(va, n, offset);
	clear();
	acquire();
	p = currun();
	if(strncmp(va, "timer startclr", 14) == 0) {
		mreset();
		memset(&C, 0, sizeof(C));
		C.timing = 1;
		p->xec = bxec;
	} else if(strncmp(va, "timer start", 11) == 0) {
		p->xec = bxec;
		C.timing = 1;
	} else if(strncmp(va, "timer stop", 10) == 0) {
		p->xec = xec;				/* bug: stop all xec threads */
		C.timing = 0;
	} else if(strncmp(va, "timer nilop", 11) == 0) {
	} else if(strncmp(va, "timer report", 12) == 0)	/* by address */
		for(mp=vmstat.list; mp; mp=mp->link) {
			ep = mp->inst + mp->ninst;
			for(ip=mp->inst; ip<ep; ip++)
				if(ip->count > 0) {
					char *mean = ts2str(statmean(&ip->t));
					char *min = ts2str(ip->t.min);
					char *max = ts2str(ip->t.max);
					char *std = ts2str(sdev(&ip->t));
					log("%s %d %s %ld %s %s %s %s\n", mp->path, ip->pc, opname(ip->op), ip->count, mean, min, max, std);
				}
		}
	else if(strncmp(va, "timer summary", 13) == 0) {	/* by opcode */
		static Stat T[MAXDIS];
		int i;

		for(i=0; i<MAXDIS; i++)
			stat(Reset, &T[i], 0);
		for(mp=vmstat.list; mp; mp=mp->link) {
			ep = mp->inst + mp->ninst;
			for(ip=mp->inst; ip<ep; ip++)
				if(ip->count > 0)
					statinc(&T[ip->op], &ip->t);
		}
		for(i=0; i<MAXDIS; i++) {
			Stat *t = &T[i];
			char *mean = "0.00 ms";
			char *min = "0.00 ms";
			char *max = "0.00 ms";
			char *std = "0.00 ms";
			if(t->n > 0) {
				mean = ts2str(statmean(t));
				min = ts2str(t->min);
				max = ts2str(t->max);
				std = ts2str(sdev(t));
			}
			log("%d %s %lld %s %s %s %s\n", i, opname(i), t->n, mean, min, max, std);
		}
	} else
		n = 0;
	R.IC = 1;
	release();

	return n;
}
 
/*
 * Garbage collection
 */
static int nidle;


/* Measure completed epochs using the unsigned epoch counter across selector wrap. */
int
idlegc(void *p)
{
	ulong epoch;
	Prog *head;
	vlong t0, t1, tot;
	USED(p);

	head = progn(0);	/* isched.head */
	epoch = gcepochs;
	tot = 0;
	while((ulong)(gcepochs-epoch) < 3 && gcruns()) {
		if(tready(nil))
			break;
		t0 = archrdtsc();
		rungc(head);
		t1 = archrdtsc();
		t1 = tickspan(t1, t0);
		if(tot > 0x7fffffffffffffffLL-t1)
			tot = 0x7fffffffffffffffLL;
		else
			tot += t1;
//		log(" %.2f",  ts2us(t1));
	}
	log(" %.2f",  ts2us(tot));
	nidle--;
	if(nidle == 0) {
		log("\n");
		return 1;
	}
	return 0;
}

static long
gctime(int report, void *va, long n, ulong offset)
{
	int i;
	vlong t0, t1;
	Prog *head;

	if(report)
		return rep(va, n, offset);
	clear();
	acquire();
	head = progn(0);	/* isched.head */
/*
	if(strncmp(va, "idle", 4) == 0) {
		nidle = 100;
		log("GCIDLE:1l:Observation:n:Time:us");
		atidle(idlegc, 0);
	} else if(strncmp(va, "stop", 4) == 0) {
		atidledont(idlegc, 0);
	} else 
*/
	if(strncmp(va, "sched", 5) == 0) {
		log("GCSCHED:1l:Observation:n:Time:us");
		for(i=0; i<1000; i++) {
			t0 = archrdtsc();
			rungc(head);
			t1 = archrdtsc();
			log(" %.2f",  ts2us(tickspan(t1, t0)));
			release();
			acquire();
		}
		log("\n");
	} else if(strncmp(va, "acquire", 7) == 0) {
		log("GCACQUIRE:1l:Observation:n:Time:us");
		for(i=0; i<1000; i++) {
			t0 = archrdtsc();
			release();
			acquire();
			head = progn(0);	/* isched.head */
			rungc(head);
			release();
			acquire();
			t1 = archrdtsc();
			log(" %.2f",  ts2us(tickspan(t1, t0)));
		}
		log("\n");
	}

	release();

	return n;
}


/* 
 * Request the number of time stamp ticks per millisecond
 */
static long
ms2ts(int report, void *va, long n, ulong offset)
{
	if(report)
		return rep(va, n, offset);
	log("%lld\n", MS2TS);
	return n;
}

/*
 * test
 */

static long
test(int report, void *va, long n, ulong offset)
{
//	vlong v;
	double d;
	if(report)
		return rep(va, n, offset);
//	v = 5;
//	print("vlong %lld\n", v);
//	print("before cast\n");
//	d = (double)v;
//	print("after cast\n");
//	print("before assign\n");
	d=100.0;
	print("after assign\n");
	print("double %f\n", d);
//	log("%lld %f\n", v, d);
	return n;
}

/*
 * $Bench builtin support
 */
void
Bench_reset(void *)
{
	bus(1);
}

void
Bench_microsec(void *fp)
{
	F_Bench_microsec *f;

	f = fp;
	*f->ret = bus(0);
}

void
Bench_disablegc(void *)
{
	gclock();
}

void
Bench_enablegc(void *)
{
	gcunlock();
}


#define fdchk(x)	((x) == (Bench_FD*)H ? -1 : (x)->fd)
void
Bench_read(void *fp)
{
	int n;
	F_Bench_read *f;
	vlong usrelease, uskread, usacquire, ussched;

	f = fp;
	n = f->n;
	if(f->buf == (Array*)H) {
		*f->ret = 0;
		return;		
	}
	if(n > f->buf->len)
		n = f->buf->len;

	bus(1);
	release();
	usrelease = bus(0);
	*f->ret = kread(fdchk(f->fd), f->buf->data, n);
	uskread = bus(0);
	acquire();
	usacquire = bus(0);
	sched();
	ussched = bus(0);
	log("%lld %lld %lld %lud %lld\n", usrelease, uskread, usacquire, m->ticks, ussched);
}


/*
 * driver support
 */
long (*Test[])(int report, void *va, long n, ulong offset) = {
	[None]		notest,
	[Calibrate]	cal,
	[Base]		base,
	[Op]			timeop,
	[Intr]			intrtime,
	[Dis]			distime,
	[Gc]			gctime,
	[MS2T]		ms2ts,
	[xTest]		test,
};

enum {
	Benchdirqid,
	Benchdataqid,
	Benchctlqid,
	Benchusqid,
};
#define Data 0
static Dirtab benchtab[]={
	".",		{Benchdirqid,0,QTDIR},	0,	0555,
	"bdata",	{Benchdataqid},		0,	0444,
	"bctl",	{Benchctlqid},		0,	0660,
	"busec",	{Benchusqid},		0,	0660,
};

static void
benchreset(void)
{
	builtinmod("$Bench", Benchmodtab);
}

static Chan*
benchattach(char *spec)
{
	bench.inuse++;
	if(bench.inuse == 1) {
		bench.bufsz = 100*READSTR;
		bench.buf = xalloc(bench.bufsz);
		bench.wpos = bench.buf;
		if(bench.buf == 0)
			error(Enomem);
		bench.test = None;
		cal(0, 0, 0, 0);
	}	
	return devattach('x', spec);
}

void
benchshutdown(void)
{
	bench.inuse--;
	if(bench.inuse == 0)
		xfree(bench.buf);
}

static Walkqid*
benchwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, benchtab, nelem(benchtab), devgen);
}

static Chan*
benchopen(Chan *c, int omode)
{
	if(c->qid.path == Benchdirqid){
		if(omode != OREAD)
			error(Eperm);
	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static int
benchstat(Chan *c, uchar *dp, int n)
{
	switch((ulong)c->qid.path){
	case Benchdataqid:
		benchtab[Data].length = bench.wpos - bench.buf;
	}
	return devstat(c, dp, n, benchtab, nelem(benchtab), devgen);
}

static void
benchclose(Chan*)
{
}


/* Honor read offsets and expose a bounded zero value until timer calibration is available. */
static long
benchread(Chan *c, void *buf, long n, vlong offset)
{
	vlong us;
	char tmp[64];

	switch((ulong)c->qid.path){
	case Benchdirqid:
		return devdirread(c, buf, n, benchtab, nelem(benchtab), devgen);

	case Benchdataqid:
		return Test[bench.test](1, buf, n, offset);

	case Benchusqid:
		us = tickmicro((uvlong)archrdtsc());
		snprint(tmp, sizeof(tmp), "%lld", us);
		return readstr(offset, buf, n, tmp);
	default:
		n = 0;
		break;
	}
	return n;
}

static long	 
benchwrite(Chan *c, void *buf, long n, vlong offset)
{
	int argn = n;

	switch((ulong)c->qid.path){
	case Benchctlqid:
		bench.test = None;
		memset((char *)bench.buf, 0, bench.bufsz);
		bench.wpos = bench.buf;
		if(strncmp(buf, "test", 4) == 0)
			bench.test = xTest;
		else if(strncmp(buf, "calibrate", 9) == 0)
			bench.test = Calibrate;
		else if(strncmp(buf, "base", 4) == 0)
			bench.test = Base;
		else if(strncmp(buf, "intr", 4) == 0)
			bench.test = Intr;
		else if(strncmp(buf, "op ", 3) == 0) {
			bench.test = Op;
			buf = (char *)buf + 3;
			argn -= 3;
		} else if(strncmp(buf, "dis ", 4) == 0) {
			bench.test = Dis;
			buf = (char *)buf + 4;
			argn -= 4;
		} else if(strncmp(buf, "gc ", 3) == 0) {
			bench.test = Gc;
			buf = (char *)buf + 3;
			argn -= 3;
		} else if(strncmp(buf, "ms2ts", 5) == 0)
			bench.test = MS2T;
		else
			error(Ebadctl);
		Test[bench.test](0, buf, argn, offset);
		break;
	case Benchusqid:
		bench.tickstart = archrdtsc();
		break;
	default:
		error(Ebadusefd);
	}
	return n;
}

Dev	benchdevtab = {
	'x',
	"bench",

	benchreset,
	devinit,
	benchshutdown,
	benchattach,
	benchwalk,
	benchstat,
	benchopen,
	devcreate,
	benchclose,
	benchread,
	devbread,
	benchwrite,
	devbwrite,
	devremove,
	devwstat,

};
