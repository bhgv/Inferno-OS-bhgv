#ifndef NATIVE64_GEOMETRY_RESULT_H
#define NATIVE64_GEOMETRY_RESULT_H
/* Private imported-result contracts: raw Word32 Draw.Point/Rect, never roots. */
static unsigned int
native64geometryresultwidth(unsigned int contract)
{
 return contract==96?8:contract==112?16:0;
}
static int
native64geometryresultmap(unsigned int bytes,const uchar*map,unsigned int np,long offset,unsigned int contract)
{
 unsigned int width=native64geometryresultwidth(contract),i,bit;
 if(!width || offset<64 || offset%4 || (unsigned long)offset>bytes || width>bytes-offset || map==nil || map==H || np!=(bytes+63)/64)return 0;
 for(i=0;i<width;i++){bit=(offset+i)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
 return 1;
}
#endif
