#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-scalar-client.h"
static unsigned char data[1048576],client[1048576];
static unsigned int readfile(char *name,unsigned char *p){FILE*f=fopen(name,"rb");unsigned int n;if(!f)exit(2);n=fread(p,1,1048576,f);if(!feof(f)||ferror(f))exit(2);fclose(f);return n;}
int main(int argc,char **argv){
 Dis64ScalarClientView v,original,c;unsigned int n;int valid,expected;
 if(argc<4)return 2;n=readfile(argv[1],data);memset(&v,0xa5,sizeof(v));original=v;expected=atoi(argv[3]);
 if(!strcmp(argv[2],"match")){
  if(argc!=5 || !dis64commandcheck(data,n,&v))return 2;
  if(!dis64clientcheck(client,readfile(argv[4],client),&c))return 2;
  valid=dis64commandmatch(&v,client+c.requests.offset,c.requests.bytes,client+c.names.offset,c.names.bytes);
 }else valid=!strcmp(argv[2],"library")?dis64commandcheck(data,n,&v):dis64clientcheck(data,n,&v);
 if(valid!=expected || (!valid && strcmp(argv[2],"match") && memcmp(&v,&original,sizeof(v)))){fprintf(stderr,"ordinary init raw proof mismatch\n");return 10;}return 0;
}
