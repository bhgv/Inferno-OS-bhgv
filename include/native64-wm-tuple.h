#ifndef NATIVE64_WM_TUPLE_H
#define NATIVE64_WM_TUPLE_H
/* Closed native result shapes used by Wmlib: String/Word, Rect/Word,
 * String/Image/String. Contracts describe layout, independently of callee name. */
static unsigned int
native64wmtuplewidth(unsigned int c)
{return c==80?16:c==92?20:c==124?24:0;}
static int
native64wmtupleroot(unsigned int c,unsigned int off)
{return c==80?off<8:c==124&&off<24;}
static int
native64wmtuplemap(unsigned int bytes,const uchar*map,unsigned int np,long off,unsigned int c)
{unsigned int n=native64wmtuplewidth(c),i,bit;if(!n||off<64||off%8||(unsigned long)off>bytes||n>bytes-off||map==nil||map==H||np!=(bytes+63)/64)return 0;for(i=0;i<n;i++){bit=(off+i)/8;if(!!(map[bit/8]&(128>>(bit%8)))!=native64wmtupleroot(c,i))return 0;}return 1;}
#endif
