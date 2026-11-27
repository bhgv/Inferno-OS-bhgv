#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-scalar-client.h"
static unsigned char data[1048576];
int main(int argc,char**argv){
 Dis64ScalarClientView view,original;FILE*f;size_t n;int valid,expected;
 if(argc!=3)return 2;f=fopen(argv[1],"rb");if(!f)return 2;n=fread(data,1,sizeof(data),f);if(!feof(f)||ferror(f))return 2;fclose(f);
 memset(&view,0xa5,sizeof(view));original=view;valid=dis64clientcheck(data,n,&view);expected=atoi(argv[2]);
 if(valid!=expected || (!valid && memcmp(&view,&original,sizeof(view)))){fprintf(stderr,"client raw proof mismatch: expected=%d actual=%d\n",expected,valid);return 10;}return 0;
}
