#ifndef NATIVE64_FILEIO_LAYOUT_H
#define NATIVE64_FILEIO_LAYOUT_H
/* Exact native Sys.FileIO, ALT replies and numeric/String result spans.
 * Result contract 16 and channel contract 136 share a map, not call semantics. */
static unsigned int
native64fileiowidth(unsigned int contract)
{return (contract==16 || contract==136)?16:contract==104?32:contract==120?24:0;}
static int
native64fileioroot(unsigned int contract,unsigned int offset)
{return (contract==16 || contract==136)?(offset>=8 && offset<16):contract==104?((offset>=8 && offset<16) || (offset>=24 && offset<32)):contract==120 && offset>=16 && offset<24;}
#endif
