#ifndef NATIVE64_LIBRARY_LAYOUT_H
#define NATIVE64_LIBRARY_LAYOUT_H
/* Closed user-ADT layouts shared by compiler and checked IR materializer.
 * IDs identify complete layouts, never an arbitrary caller-provided offset. */
enum { Native64LibraryRecords=5 };
static unsigned int native64librarybytes(unsigned int id){return id==1?56:id>=2&&id<=5?24:0;}
static unsigned int native64librarymap(unsigned int id){return id>=1&&id<=3?0xc0:id<=5&&id>=4?0xe0:0;}
static unsigned int native64libraryfields(unsigned int id){return id==1?9:id>=2&&id<=5?3:0;}
static unsigned int native64libraryold(unsigned int id,unsigned int field){static const unsigned int old[]={0,4,8,12,16,24,32,40,44};return id==1&&field<9?old[field]:id>=2&&id<=5&&field<3?4*field:~0U;}
static unsigned int native64libraryoffset(unsigned int id,unsigned int field){static const unsigned int off[]={0,8,16,20,24,32,40,48,52};return id==1&&field<9?off[field]:id>=2&&id<=5&&field<3?8*field:~0U;}
static unsigned int native64librarywidth(unsigned int id,unsigned int field){return id==1&&field<9?(field<2||field==5||field==6?8:4):id>=2&&id<=5&&field<3?(id<=3&&field==2?4:8):0;}
static int native64libraryroot(unsigned int id,unsigned int field){return id==1?field<2:id>=2&&id<=3?field<2:id>=4&&id<=5&&field<3;}
#endif
