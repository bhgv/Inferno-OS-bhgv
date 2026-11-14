/* Appended to the allocation-accounted typed IR fixture. */
#include "dis64-call-infer.h"
static uchar captured[1048576];static long capturedbytes;static int rejected;
Decl *impdecl;Biobuf *bout;
long Bwrite(Biobuf *b,void *p,long n){USED(b);assert(n>0 && n<=sizeof(captured));memmove(captured,p,n);capturedbytes=n;return n;}
void error(Line src,char *format,...){(void)src;USED(format);rejected++;}
static Sym *callsymbol(char *name){Sym *s=allocmem(sizeof(*s)+strlen(name)+1);s->name=(char*)(s+1);s->len=strlen(name);strcpy(s->name,name);return s;}
int main(void)
{
    Type word={0},none={0},rootfn={0},leaffn={0},module={0},iface={0};Decl root={0},leaf={0},arg={0},locals[3]={{0}},mod={0},*functions[]={&root,&leaf};
    Desc rd={0},ld={0},data={0};Inst code[8]={{0}},save;unsigned i,baseline;Dis64CallImage checked;
    typedbase();word.kind=Tint;word.size=word.align=4;none.kind=Tnone;rootfn.kind=leaffn.kind=Tfn;rootfn.tof=&none;leaffn.tof=&word;
    arg.store=Darg;arg.ty=&word;arg.offset=32;leaffn.ids=&arg;
    root.store=leaf.store=Dfn;root.ty=&rootfn;leaf.ty=&leaffn;root.next=&leaf;root.pc=code;leaf.pc=code+6;
    root.sym=callsymbol("init");leaf.sym=callsymbol("leaf");mod.sym=callsymbol("Calls");module.kind=Tmodule;module.tof=&iface;iface.ids=&root;mod.ty=&module;impdecl=&mod;
    rd.size=48;rd.id=1;ld.size=40;ld.id=2;root.desc=&rd;leaf.desc=&ld;descriptors=&data;
    for(i=0;i<3;i++){locals[i].store=Dlocal;locals[i].ty=&word;locals[i].offset=32+4*i;locals[i].next=i<2?locals+i+1:nil;}root.locals=locals;
    for(i=0;i<8;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<7?code+i+1:nil;}
    code[0].op=IFRAME;code[0].sm=Adesc;code[0].s.offset=2;code[0].dm=Afp;code[0].d.reg=44;
    code[1].op=IMOVW;code[1].sm=Aimm;code[1].s.offset=42;code[1].dm=Afpind;code[1].d.reg=44;code[1].d.offset=32;
    code[2].op=ILEA;code[2].sm=Afp;code[2].s.reg=32;code[2].dm=Afpind;code[2].d.reg=44;code[2].d.offset=16;
    code[3].op=ICALL;code[3].sm=Afp;code[3].s.reg=44;code[3].dm=Apc;code[3].d.offset=6;
    code[4].op=IMOVW;code[4].sm=Afp;code[4].s.reg=32;code[4].dm=Afp;code[4].d.reg=36;code[5].op=IRET;
    code[6].op=IMOVW;code[6].sm=Afp;code[6].s.reg=32;code[6].dm=Afpind;code[6].d.reg=16;code[7].op=IRET;
    baseline=owners;dis64callsfile(&root,nil,functions,2,code,8);
    assert(!rejected && capturedbytes>0 && dis64callimageopen(captured,capturedbytes,1048576,&checked)); /* WRITER_POSITIVE */
    assert(owners==baseline); /* WRITER_OWNERS */
    capturedbytes=0;save=code[1];code[1].sm=Afp;code[1].s.reg=36; /* Typed int local, never initialized in raw source. */
    dis64callsfile(&root,nil,functions,2,code,8);
    assert(rejected==1 && capturedbytes==0); /* WRITER_PROOF */
    assert(owners==baseline);code[1]=save;
    capturedbytes=0;rejected=0;dis64callsfile(nil,nil,functions,2,code,8);assert(rejected==1 && !capturedbytes && owners==baseline);
    typedfree(root.sym);typedfree(leaf.sym);typedfree(mod.sym);assert(owners==0);return 0;
}
