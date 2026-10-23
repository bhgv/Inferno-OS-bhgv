#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "mp.h"
#include "libsec.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#define DIGITS(n) (((n)+Dbits-1)/Dbits)
#define mpdighi ((mpdigit)1<<(Dbits-1))
static int live,checks,convertcalls,failconvert=-1;
enum { MaxBigBytes=1024 };

/* Every real key/digit allocation must be released after accepted or rejected input. */
static void *trackedmalloc(size_t n) {void *p=malloc(n?n:1);assert(p);live++;return p;}
static void trackedfree(void *p) {if(p) {assert(live>0);live--;free(p);}}
static void *trackedrealloc(void *p,size_t n) {if(!p)return trackedmalloc(n);p=realloc(p,n);assert(p);return p;}
void *mallocz(ulong n,int clear) {void *p=trackedmalloc(n);if(clear)memset(p,0,n);return p;}
void sysfatal(char *text,...) {(void)text;abort();}
int __lsan_is_turned_off(void) {return 1;}
#define malloc trackedmalloc
#define free trackedfree
#define realloc trackedrealloc
NATIVE_IMPLEMENTATION

/* Simulate a failed numeric import after earlier fields acquired real digit owners. */
static mpint *counted_betomp(uchar *data,uint n,mpint *b) {
	convertcalls++;
	if(failconvert==0)return nil;
	if(failconvert>0)failconvert--;
	return betomp(data,n,b);
}
#define betomp counted_betomp
PARSER_IMPLEMENTATION
#undef betomp
ALGORITHM_IMPLEMENTATION
VECTORS

/* Compare whole decoded values, including boundaries where old dec64 truncated. */
static void base64values(void) {
	mpint *b;uchar result[800];char *next,*text;int i,j,n;
	for(i=0;i<(int)nelem(vectors);i++) {
		text=vectors[i].text;next=(char*)1;b=base64tobig(text,&next);
		if(vectors[i].size>768) {assert(b==nil && next==text);checks++;continue;}
		assert(b && next==text+strlen(text));n=mptobe(b,result,sizeof(result),nil);
		assert(n==vectors[i].size);
		for(j=0;j<n;j++)assert(result[j]==(uchar)(j*37+1));
		mpfree(b);assert(live==0);checks++;
	}
	text="AQ\nnext";b=base64tobig(text,&next);assert(b && !strcmp(next,"next"));mpfree(b);checks++;
	text="AA==";b=base64tobig(text,&next);assert(b && b->top==0);mpfree(b);checks++;
}
/* Invalid fields leave an initialized cursor and never invoke numeric import. */
static void base64errors(void) {
	char *bad[]={"","\n","A","=","!!!!","AQ===","AQ=","A===","AAAA=","AA=A","AQ==x","AQ== "," A Q==","AQ\r","\xff"};
	char *p;int i;
	for(i=0;i<(int)nelem(bad);i++) {
		p=(char*)1;convertcalls=0;assert(base64tobig(bad[i],&p)==nil);
		assert(p==bad[i] && convertcalls==0 && live==0);checks++;
	}
	p=(char*)1;assert(base64tobig(nil,&p)==nil && p==nil);checks++;
}
/* View each actual algorithm layout explicitly so duplicated DSA fields are visible. */
static void fields(void *key,int alg,int kind,mpint **out) {
	if(alg==0) {
		if(kind==0) {RSApriv *k=key;out[0]=k->pub.n;out[1]=k->pub.ek;out[2]=k->dk;out[3]=k->p;out[4]=k->q;out[5]=k->kp;out[6]=k->kq;out[7]=k->c2;}
		else if(kind==1) {RSApub *k=key;out[0]=k->n;out[1]=k->ek;}
		else out[0]=key;
	}else if(alg==1) {
		if(kind==0) {DSApriv *k=key;out[0]=k->pub.p;out[1]=k->pub.q;out[2]=k->pub.alpha;out[3]=k->pub.key;out[4]=k->secret;}
		else if(kind==1) {DSApub *k=key;out[0]=k->p;out[1]=k->q;out[2]=k->alpha;out[3]=k->key;}
		else {DSAsig *k=key;out[0]=k->r;out[1]=k->s;}
	}else{
		if(kind==0) {EGpriv *k=key;out[0]=k->pub.p;out[1]=k->pub.alpha;out[2]=k->pub.key;out[3]=k->secret;}
		else if(kind==1) {EGpub *k=key;out[0]=k->p;out[1]=k->alpha;out[2]=k->key;}
		else {EGsig *k=key;out[0]=k->r;out[1]=k->s;}
	}
}
/* Match each parser with its real cryptographic destructor. */
static void freekey(void *key,int alg,int kind) {
	if(alg==0) {if(kind==0)rsaprivfree(key);else if(kind==1)rsapubfree(key);else mpfree(key);}
	else if(alg==1) {if(kind==0)dsaprivfree(key);else if(kind==1)dsapubfree(key);else dsasigfree(key);}
	else {if(kind==0)egprivfree(key);else if(kind==1)egpubfree(key);else egsigfree(key);}
}
/* Exhaust missing/bad/import-failure positions for every public, private and signature form. */
static void algorithms(void) {
	void *(*parsers[3][3])(char*,char**)={
		{rsa_str2sk,rsa_str2pk,rsa_str2sig},{dsa_str2sk,dsa_str2pk,dsa_str2sig},{eg_str2sk,eg_str2pk,eg_str2sig}};
	int counts[3][3]={{8,2,1},{5,4,2},{4,3,2}};
	char *lines[]={"AQ==\n","Ag==\n","Aw==\n","BA==\n","BQ==\n","Bg==\n","Bw==\n","CA==\n"};
	char buffer[80],*next;void *key;mpint *values[8];int alg,kind,n,i,cut;
	for(alg=0;alg<3;alg++)for(kind=0;kind<3;kind++) {
		n=counts[alg][kind];buffer[0]=0;for(i=0;i<n;i++)strcat(buffer,lines[i]);strcat(buffer,"tail");
		convertcalls=0;key=parsers[alg][kind](buffer,&next);
		assert(key && next==buffer+n*5 && convertcalls==n);
		fields(key,alg,kind,values);
		for(i=0;i<n;i++)assert(values[i]->top==1 && values[i]->p[0]==(mpdigit)(i+1));
		freekey(key,alg,kind);assert(live==0);checks++;
		for(cut=0;cut<n;cut++) {
			buffer[0]=0;for(i=0;i<cut;i++)strcat(buffer,lines[i]);
			next=(char*)1;convertcalls=0;assert(parsers[alg][kind](buffer,&next)==nil);
			assert(next==buffer && live==0 && convertcalls==cut);checks++;
			buffer[0]=0;for(i=0;i<n;i++)strcat(buffer,i==cut?"!bad\n":lines[i]);
			next=(char*)1;convertcalls=0;assert(parsers[alg][kind](buffer,&next)==nil);
			assert(next==buffer && live==0 && convertcalls==cut);checks++;
			buffer[0]=0;for(i=0;i<n;i++)strcat(buffer,lines[i]);
			failconvert=cut;convertcalls=0;next=(char*)1;
			assert(parsers[alg][kind](buffer,&next)==nil && live==0 && next==buffer);
			assert(convertcalls==cut+1);failconvert=-1;checks++;
		}
		next=(char*)1;assert(parsers[alg][kind](nil,&next)==nil && next==nil && live==0);checks++;
	}
}
/* Check data integrity and owned partial cleanup without generating real secret keys. */
int main(void) {
	base64values();base64errors();algorithms();assert(live==0);
	printf("Keyring keys: %d exact-value/parser/cleanup checks passed\n",checks);return 0;
}
