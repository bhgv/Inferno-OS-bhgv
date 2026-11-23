#ifndef NATIVE64_ARRAY_LAYOUT_H
#define NATIVE64_ARRAY_LAYOUT_H
/* Private layout classes: existing raw records, 1..3 roots, or roots followed
 * by 1..2 Words. A nested aggregate may pad between the two Words.
 * Bits: 4=mixed, 2..3=root count, 0=Word count minus one,
 * 5=padding between Words; bit 1 is reserved. Classes 1..3 remain raw. */
enum { Native64ArrayRoot = 4 };
enum { Native64TypedBorrowIndex = 36, Native64TypedArrayArgIndex = 37 };
static unsigned int native64arrayroots(unsigned int c)
{
 if(c==4||c==8||c==12)return c/4;
 if(!(c&16)||(c&~63U)||(c&3)>=2||((c&32)&&!(c&3)))return 0;
 return (c>>2)&3;
}
static unsigned int native64arraywords(unsigned int c)
{return (c&16)&&native64arrayroots(c)?1+(c&3):0;}
static int native64arrayrecord(unsigned int c)
{return c!=Native64ArrayRoot&&native64arrayroots(c)>0;}
static unsigned int native64arraywordoffset(unsigned int c,unsigned int i)
{return 8*native64arrayroots(c)+4*i+(i&&(c&32)?4:0);}
static unsigned int native64arraymap(unsigned int c)
{unsigned int n=native64arrayroots(c);return n?(255U<<(8-n))&255:0;}
static unsigned int native64arraywidth(unsigned int c)
{
 unsigned int n=native64arrayroots(c),w=native64arraywords(c);
 if(w)return (native64arraywordoffset(c,w-1)+11)&~7U;
 if(n)return 8*n;
 return c==1?4:c==2?12:c==3?16:0;
}
static unsigned int native64arraylegacy(unsigned int c)
{return native64arrayroots(c)?4*(native64arrayroots(c)+native64arraywords(c)):native64arraywidth(c);}
static unsigned int native64arrayword(unsigned int c,unsigned int off)
{unsigned int i;if(!native64arrayroots(c))return off%4==0&&off<native64arraywidth(c);for(i=0;i<native64arraywords(c);i++)if(off==native64arraywordoffset(c,i))return 1;return 0;}
/* A mapped copy must preserve active fields; padding cannot cover another Word. */
static int native64arraycopyspan(unsigned int owner,unsigned int off,unsigned int copy)
{
 unsigned int i,n=native64arraywidth(copy),width=native64arraywidth(owner),root;
 if(!native64arrayrecord(copy)||off%8||off>width||n>width-off)return 0;
 for(i=0;i<n;i+=4){root=i<8*native64arrayroots(copy);
  if(root!=(off+i<8*native64arrayroots(owner)))return 0;
  if(!root&&native64arrayword(copy,i)!=native64arrayword(owner,off+i))return 0;
 }return 1;
}
#endif
