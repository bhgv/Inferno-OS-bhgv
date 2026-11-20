#ifndef NATIVE64_FILEIO_LAYOUT_H
#define NATIVE64_FILEIO_LAYOUT_H
/* Private ALT contracts: exact native Sys.FileIO and (int,String) reply payload spans and GC bytes. */
static unsigned int
native64fileiowidth(unsigned int contract)
{return contract==136?16:contract==104?32:contract==120?24:0;}
static int
native64fileioroot(unsigned int contract,unsigned int offset)
{return contract==136?(offset>=8 && offset<16):contract==104?((offset>=8 && offset<16) || (offset>=24 && offset<32)):contract==120 && offset>=16 && offset<24;}
#endif
