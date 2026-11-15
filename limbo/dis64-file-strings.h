/* Trusted globals -> the existing private MP24+String / trailing Rune pool.
 * The complete independently checked output remains the admission boundary. */
typedef struct FileStrings FileStrings;
struct FileStrings {
 Decl *nilvalue,*emptyvalue,*decls[Dis64ManagedLiterals+1];
 Sym *values[Dis64ManagedLiterals+1];
 long offsets[Dis64ManagedLiterals+1],niloffset,emptyoffset;
 unsigned int count,literals;int path;
};
static int
filestringscollect(FileStrings *p,Decl *globals,long bytes)
{
 Decl *d;Sym *s;memset(p,0,sizeof(*p));p->path=-1;
 for(d=globals;d;d=d->next){
  if(d->ty->kind==Tany && !d->init && d->sym && !strcmp(d->sym->name,"nil")){
   if(p->nilvalue)return 0;p->nilvalue=d;continue;
  }
  if(d->ty->kind!=Tstring)return 0;
  if(d->init && d->init->op==Oconst && d->init->decl)s=d->init->decl->sym;
  else if(!d->init && d->sym && !d->sym->len){if(p->emptyvalue)return 0;p->emptyvalue=d;continue;}else return 0;
  if(!s || p->count==Dis64ManagedLiterals+1)return 0;
  p->decls[p->count]=d;p->values[p->count++]=s;
 }
 return bytes==4*(p->count+(p->nilvalue!=nil)+(p->emptyvalue!=nil));
}
static int
filestringsplan(FileStrings *p,const Dis64TypedBody *body,int client)
{
 unsigned int i,j;int found;
 if(body->data.count!=p->count+(p->nilvalue!=nil)+(p->emptyvalue!=nil))return 0;
 for(i=0;i<body->data.count;i++){
  const Dis64LayoutLeaf *leaf=body->data.leaves+i;found=0;
  if(p->nilvalue && leaf->legacy==(unsigned long)p->nilvalue->offset && leaf->kind==Tany){p->niloffset=leaf->native;continue;}
  if(p->emptyvalue && leaf->legacy==(unsigned long)p->emptyvalue->offset && leaf->kind==Tstring){p->emptyoffset=leaf->native;continue;}
  for(j=0;j<p->count;j++)if(leaf->kind==Tstring && leaf->legacy==(unsigned long)p->decls[j]->offset){p->offsets[j]=leaf->native;found=1;break;}
  if(!found)return 0;
 }
 if(client){
  for(i=0;i<body->count;i++)if(body->code[i].op==ILOAD){
   if(USRC(body->code[i].mode)!=AMP)return 0;
   for(j=0;j<p->count && p->offsets[j]!=body->code[i].source;j++);
   if(j==p->count || (p->path>=0 && p->path!=(int)j))return 0;p->path=j;
  }
  if(p->path<0)return 0;
 }
 p->literals=p->count-client;return p->literals<=Dis64ManagedLiterals;
}
static int
filestringaddress(const FileStrings *p,long source,long *target)
{
 unsigned int i,j=0;
 if((p->nilvalue && source==p->niloffset) || (p->emptyvalue && source==p->emptyoffset)){*target=8;return 1;}
 for(i=0;i<p->count;i++){
  if((int)i==p->path){if(source==p->offsets[i]){*target=16;return 1;}continue;}
  if(source==p->offsets[i]){*target=24+8*j;return 1;}j++;
 }
 return 0;
}
/* Same bounded Rune32 convention as the earlier managed-file literal writer. */
static int
filestringpool(const FileStrings *p,uchar *runes,DisStringPoolRecord *records,unsigned int base)
{
 unsigned int i,j=0,at,count,used,total=0;Rune rune;Sym *s;uchar *out;
 for(i=0;i<p->count;i++)if((int)i!=p->path){
  s=p->values[i];if(s->len<0 || s->len>4*255)return 0;
  out=runes+(base+j)*(4+4*1023);at=count=0;
  while(at<(unsigned int)s->len){
   used=chartorune(&rune,s->name+at);if(!used || used>(unsigned int)s->len-at || count==255)return 0;
   dis64put32(out+4+4*count++,rune);at+=used;
  }
  if(count>Dis64ManagedLiteralRunes-total)return 0;total+=count;
  dis64put32(out,count);records[base+j++]=(DisStringPoolRecord){out,4+4*count};
 }
 return j==p->literals;
}
static unsigned int
filestringtypes(const FileStrings *p,Dis64Type *types,uchar *maps)
{
 unsigned int i;for(i=0;i<3+p->literals;i++)maps[64+i/8]|=0x80U>>(i%8);maps[70]=0xe0;
 types[1]=(Dis64Type){24+8*p->literals,8,maps+64,(3+p->literals+7)/8};
 types[2]=(Dis64Type){24,8,maps+70,1};return p->literals?3:2;
}
