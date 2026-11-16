#ifndef DIS64_SYS_MOUNT_SCHEMA_H
#define DIS64_SYS_MOUNT_SCHEMA_H
/* Private selected Sys.mount tree; no public managed kind assignment. */
static unsigned int
dis64sysmountsignature(unsigned char *out)
{
    static const unsigned int words[]={0xface0010U,5,Dis64Int,
        0x73,0x74,1,Dis64Int,0x73,0x74,1,Dis64Int,0x71,Dis64Int,0x71};
    unsigned int i;
    for(i=0;i<sizeof(words)/sizeof(words[0]);i++)dis64put32(out+4*i,words[i]);
    return sizeof(words);
}
static unsigned int
dis64connectionargs(unsigned int kind)
{
    return kind==15?5:kind==14?2:dis64sysfixedargs(kind);
}
static unsigned int
dis64connectionoffset(unsigned int kind,unsigned int argument)
{
    static const unsigned int mount[]={64,72,80,88,96};
    /* Create packs its two Words at72/76; preserve prior profile bytes. */
    return kind==15?mount[argument]:kind==11?dis64sysfixedoffset(kind,argument):64+8*argument;
}
static int
dis64connectionstring(unsigned int kind,unsigned int argument)
{
    return kind==14 || kind<3 || kind==6 || (kind==7 && argument<2) || ((kind==10 || kind==11) && !argument) || (kind==15 && (argument==2 || argument==4));
}
static int
dis64connectionfd(unsigned int kind,unsigned int argument)
{
    return kind==15?argument<2:kind==9 || ((kind==12 || kind==13) && !argument);
}
#endif
