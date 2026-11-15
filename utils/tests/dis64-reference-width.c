/* Isolated actual-handler oracle. These are value bytes, never heap addresses
 * or published GC roots. No guest file containing forged references executes. */
static void
commandreferencewidth(void)
{
 REG saved=R;Inst targets[2],*target=targets+1;unsigned int i,ne;
 unsigned long long values[][2]={
  {0x00000001ffffffffULL,0xffffffffffffffffULL},
  {0x0000000100000000ULL,0x0000000200000000ULL},
  {0x0000000000000001ULL,0x8000000000000001ULL},
  {0x7fffffff12345678ULL,0x8000000012345678ULL},
  {0xffffffffffffffffULL,0xffffffffffffffffULL},
  {0x0000000100000000ULL,0x0000000100000000ULL},
  {0,0},{0,0x0000000100000000ULL}
 };
 for(i=0;i<nelem(values);i++)for(ne=0;ne<2;ne++){
  R.s=(uchar*)&values[i][0];R.m=(uchar*)&values[i][1];R.d=(uchar*)&target;R.PC=targets;
  if(ne)actual_bnel();else actual_beql();
  if((R.PC==target)!=((values[i][0]!=values[i][1])==ne)){
   fprintf(stderr,"reference width mismatch\n");exit(10);
  }
 }
 R=saved;
}
