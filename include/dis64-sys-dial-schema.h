#ifndef DIS64_SYS_DIAL_SCHEMA_H
#define DIS64_SYS_DIAL_SCHEMA_H
/* Private tuple witness; no public kind assignment or general ADT admission. */
static unsigned int
dis64sysdialsignature(unsigned char *out)
{
    static const unsigned int words[]={0xface0010U,2,0x77,2,Dis64Int,
        0x74,3,0x73,0x74,1,Dis64Int,
        0x73,0x74,1,Dis64Int,0x71,0x71,0x71};
    unsigned int i;
    for(i=0;i<sizeof(words)/sizeof(words[0]);i++)dis64put32(out+4*i,words[i]);
    return sizeof(words);
}

#endif
