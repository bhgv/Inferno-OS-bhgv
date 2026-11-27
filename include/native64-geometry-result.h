#ifndef NATIVE64_GEOMETRY_RESULT_H
#define NATIVE64_GEOMETRY_RESULT_H
static int
native64windowgeometryfield(unsigned int field,unsigned int bytes)
{return bytes==16?(field==8||field==32||field==88):bytes==8&&(field==8||field==16||field==32||field==40||field==80||field==88||field==96||field==120);}
static int
native64windowrootfield(unsigned int field)
{return field==0||field==24||field==48||field==64||field==72||field==104;}
/* Private result contracts: raw Word32 Draw.Point/Rect, never roots. */
static unsigned int
native64geometryresultwidth(unsigned int contract)
{
 /* Contract 12 is three Word32 results; 96/112 retain Point/Rect ABI. */
 return contract==12?12:contract==96?8:contract==112?16:0;
}
static int
native64geometryresultmapat(unsigned int bytes,const uchar*map,unsigned int np,long offset,unsigned int contract,unsigned int minimum)
{
 unsigned int width=native64geometryresultwidth(contract),i,bit;
 if(!width || offset<(long)minimum || offset%4 || (unsigned long)offset>bytes || width>bytes-offset || map==nil || map==H || np!=(bytes+63)/64)return 0;
 for(i=0;i<width;i++){bit=(offset+i)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}
 return 1;
}
static int
native64geometryresultmap(unsigned int bytes,const uchar*map,unsigned int np,long offset,unsigned int contract)
{return native64geometryresultmapat(bytes,map,np,offset,contract,64);}
#endif
