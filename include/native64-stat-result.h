#ifndef NATIVE64_STAT_RESULT_H
#define NATIVE64_STAT_RESULT_H
/* Private checked-IR contract 88: (WORD status, padding, Sys.Dir).
 * Four String roots occupy result bytes 8..39; all other cells are scalar.
 * This is not a serialized Dis64 capability or a general tuple ABI. */
static int
native64statresultmap(unsigned int bytes,const uchar *map,unsigned int np,unsigned int offset)
{
 unsigned int at,bit;
 if(map==nil || map==H || offset<64 || offset%8 || offset>bytes || 88>bytes-offset)return 0;
 for(at=0;at<88;at+=8){
  bit=(offset+at)/8;
  if(bit/8>=np || !!(map[bit/8]&(0x80>>(bit%8)))!=(at>=8 && at<40))return 0;
 }
 return 1;
}
#endif
