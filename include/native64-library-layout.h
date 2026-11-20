#ifndef NATIVE64_LIBRARY_LAYOUT_H
#define NATIVE64_LIBRARY_LAYOUT_H
/* Closed user-ADT layouts shared by compiler and checked IR materializer.
 * IDs identify complete layouts, never an arbitrary caller-provided offset. */
enum { Native64LibraryRecords=11 };
static unsigned int native64librarybytes(unsigned int id){return id==11?128:id==10?48:id==8?56:id==9?8:id==6?96:id==7?32:id==1?56:id>=2&&id<=5?24:0;}
static unsigned int native64librarymap(unsigned int id){return id==11?0x92:id==10?0xf0:id==8?0xfe:id==9?0:id==6?0xf7:id==7?0x90:id>=1&&id<=3?0xc0:id<=5&&id>=4?0xe0:0;}
static unsigned int native64libraryfields(unsigned int id){return id==11?26:id==10?8:id==8?7:id==9?2:id==6?13:id==7?6:id==1?9:id>=2&&id<=5?3:0;}
static unsigned int native64libraryold(unsigned int id,unsigned int field){static const unsigned int old[]={0,4,8,12,16,24,32,40,44};return id==11&&field<26?4*field:id==10&&field<8?4*field:(id==8&&field<7)||(id==9&&field<2)?4*field:id==6&&field<13?4*field:id==7&&field<6?4*field:id==1&&field<9?old[field]:id>=2&&id<=5&&field<3?4*field:~0U;}
static unsigned int native64libraryoffset(unsigned int id,unsigned int field){static const unsigned int off[]={0,8,16,20,24,32,40,48,52};static const unsigned int client[]={0,8,16,24,32,40,48,56,64,72,76,80,88};static const unsigned int window[]={0,8,12,16,20,24,32,36,40,44,48,56,60,64,72,80,84,88,92,96,100,104,112,116,120,124};return id==11&&field<26?window[field]:id==10&&field<8?(field<4?8*field:32+4*(field-4)):id==8&&field<7?8*field:id==9&&field<2?4*field:id==6&&field<13?client[field]:id==7&&field<6?(field?4+4*field:0):id==1&&field<9?off[field]:id>=2&&id<=5&&field<3?8*field:~0U;}
static unsigned int native64librarywidth(unsigned int id,unsigned int field){return id==11&&field<26?(field==0||field==5||field==10||field==13||field==14||field==21?8:4):id==10&&field<8?(field<4?8:4):id==8&&field<7?8:id==9&&field<2?4:id==6&&field<13?(field==4||field==9||field==10||field==11?4:8):id==7&&field<6?(field==0||field==5?8:4):id==1&&field<9?(field<2||field==5||field==6?8:4):id>=2&&id<=5&&field<3?(id<=3&&field==2?4:8):0;}
static int native64libraryroot(unsigned int id,unsigned int field){return id==11?(field==0||field==5||field==10||field==13||field==14||field==21):id==10?field<4:id==8&&field<7?1:id==9?0:id==6&&field<13?!(field==4||field==9||field==10||field==11):id==7&&field<6?(field==0||field==5):id==1?field<2:id>=2&&id<=3?field<2:id>=4&&id<=5&&field<3;}
static unsigned int native64librarymapsecond(unsigned int id){return id==11?0xc4:id==6?0x90:0;}
#endif
