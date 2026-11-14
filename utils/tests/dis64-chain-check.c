#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-call-chain.h"
static unsigned int word(FILE*f){unsigned char b[4];if(fread(b,1,4,f)!=4)exit(11);return b[0]*16777216U+b[1]*65536U+b[2]*256U+b[3];}
static void check(int ok){if(!ok){fprintf(stderr,"chain predicate failed\n");exit(10);}}
int main(int argc,char**argv){
 static unsigned char data[1048576];static Dis64CallInference work;
 static Dis64CallDecoded decoded[256];static Dis64InitWork scratch[256];static unsigned int queue[256];
 struct Publication{Dis64BoundCall records[64];unsigned int count;Dis64CallImage image;}out,saved,old;
 FILE*f;unsigned int n,i,size,expected,limit,capacity;int accepted;
 check(argc==2);f=fopen(argv[1],"rb");check(f!=0);n=word(f);
 for(i=0;i<n;i++){
  size=word(f);expected=word(f);limit=word(f);capacity=word(f);check(size<=sizeof(data));check(fread(data,1,size,f)==size);
  memset(&out,0xa5,sizeof(out));saved=out;
  accepted=dis64callchaininfercheck(data,size,1048576,&work,decoded,scratch,queue,256,limit,out.records,capacity,&out.count,&out.image);
  if(accepted!=(int)expected)fprintf(stderr,"chain case %u accepted=%d expected=%u\n",i,accepted,expected);
  check(accepted==(int)expected);
  if(!accepted)check(memcmp(&out,&saved,sizeof(out))==0);
  else{
   check(out.count>0 && out.count<=capacity);
   memset(&old,0xa5,sizeof(old));
   if(dis64callinfercheck(data,size,1048576,&work,decoded,scratch,queue,256,limit,old.records,capacity,&old.count,&old.image)){
    check(out.count==old.count);check(memcmp(out.records,old.records,out.count*sizeof(out.records[0]))==0);
    check(memcmp(&out.image,&old.image,sizeof(old.image))==0);
   }
  }
 }
 check(fgetc(f)==EOF);fclose(f);return 0;
}
