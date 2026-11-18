#include <stdio.h>
static void statir(int ok,char*why){if(!ok){fprintf(stderr,"fstat IR predicate: %s\n",why);exit(10);}}
int main(void){
 Type word={0},text={0},wide={0},dir={0},fd={0},ref={0},tuple={0},fn={0};Decl fields[13]={{0}},pair[2]={{0}},fdfield={0},formal={0},owner={0},dd={0},fddecl={0},sys={0};Sym ds={0},fs={0},ss={0},method={0};Dis64Layout plan={0};Dis64TypedBody p={0};Inst bind={0};unsigned int i,base;int k;
 static const unsigned int old[]={0,4,8,12,16,24,28,32,36,40,48,56,60};
 ds.name="Dir";fs.name="FD";ss.name="Sys";method.name="fstat";sys.sym=&ss;dd.sym=&ds;dd.dot=&sys;fddecl.sym=&fs;fddecl.dot=&sys;
 word.kind=Tint;word.size=word.align=4;text.kind=Tstring;text.size=text.align=4;wide.kind=Tbig;wide.size=wide.align=8;
 dir.kind=Tadt;dir.size=64;dir.align=8;dir.ids=fields;dir.decl=&dd;dd.ty=&dir;
 for(i=0;i<13;i++){fields[i].store=Dfield;fields[i].offset=old[i];fields[i].ty=i<4?&text:i==4||i==10?&wide:&word;fields[i].next=i<12?fields+i+1:nil;}
 fd.kind=Tadt;fd.size=fd.align=4;fd.ids=&fdfield;fd.decl=&fddecl;fdfield.ty=&word;fdfield.store=Dfield;fddecl.ty=&fd;
 ref.kind=Tref;ref.size=ref.align=4;ref.tof=&fd;formal.ty=&ref;formal.store=Darg;formal.offset=32;
 tuple.kind=Ttuple;tuple.size=72;tuple.align=8;tuple.ids=pair;pair[0].ty=&word;pair[0].store=pair[1].store=Dfield;pair[0].next=pair+1;pair[1].ty=&dir;pair[1].offset=8;
 fn.kind=Tfn;fn.tof=&tuple;fn.ids=&formal;owner.ty=&fn;owner.sym=&method;owner.dot=&sys;bind.sm=Afp;bind.s.reg=40;
 statir(dis64layouttype(&tuple,&plan)&&plan.count==14&&plan.bytes==88,"exact tuple layout");
 p.frame.count=p.frame.capacity=17;p.frame.bytes=168;p.frame.legacybytes=120;p.frame.leaves=allocmem(17*sizeof(*p.frame.leaves));
 p.frame.leaves[0]=(Dis64LayoutLeaf){32,64,4,8,Tref};p.frame.leaves[1]=(Dis64LayoutLeaf){36,72,4,4,Tint};
 for(i=0;i<14;i++){p.frame.leaves[i+2]=plan.leaves[i];p.frame.leaves[i+2].legacy+=40;p.frame.leaves[i+2].native=76+plan.leaves[i].native-(i?4:0);}
 p.frame.leaves[16]=(Dis64LayoutLeaf){112,160,4,4,Tint};
 statir(!typedstatresult(&owner,&bind,&p,&base,0),"unpacked tuple refused");
 statir(typedstatresult(&owner,&bind,&p,&base,1)&&base==80&&p.frame.bytes==176&&p.frame.leaves[3].native==88&&p.frame.leaves[16].native==168,"repacked result group");
 statir(typedstatresult(&owner,&bind,&p,&base,1)&&p.frame.bytes==176,"idempotent repacking");
 for(k=0;k<8;k++){
  if(k==0)method.name="stat";if(k==1)ss.name="Other";if(k==2)ds.name="Other";if(k==3)fs.name="Other";if(k==4)fields[10].ty=&word;if(k==5)p.frame.leaves[3].kind=Tint;if(k==6)bind.s.reg=2147483647;if(k==7)p.frame.count=3;
  statir(!typedstatresult(&owner,&bind,&p,&base,0),"malformed namespace/type/leaf/span refused");method.name="fstat";ss.name="Sys";ds.name="Dir";fs.name="FD";fields[10].ty=&wide;p.frame.leaves[3].kind=Tstring;bind.s.reg=40;p.frame.count=17;
 }
 dis64layoutfree(&p.frame);dis64layoutfree(&plan);statir(!owners,"compiler owners retired");puts("PASS fstat IR: exact mixed tuple, relocated prefix/tail, idempotence and eight malformed type/leaf/span refusals");return 0;
}
