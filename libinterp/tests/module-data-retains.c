/* Module-data preflight must consume only its module reference on rejection. */
int
main(void)
{
	uchar *payload, savedpayload[16];

	struct {
		Heap header; // Real owner prefix required by the temporary-root boundary.
		uchar bytes[16]; // Borrowed original MP; preflight must leave these bytes unchanged.
	} source;
	Type beforetype,beforemodlink;
	Type *selected;
	int mode,ok,valid;
	char *modevalue=getenv("MODULE_DATA_MODE");
	ulong total=0;

	payload = source.bytes;
	for(mode=0;mode<17;mode++){
		if(modevalue && mode!=atoi(modevalue))continue;
		setup();memset(&module,0,sizeof(module));memset(imports,0,sizeof(imports));
		memset(&source, 0, sizeof(source));
		source.header.ref = 1;
		source.header.t = &mp;
		memset(payload,0x5a,sizeof(source.bytes));memmove(savedpayload,payload,sizeof(source.bytes));
		mp.size=sizeof(source.bytes);mp.np=0;mp.ref=1;selected=&mp;moduletypes[0]=selected;
		module.ref=2;module.origmp=payload;module.type=moduletypes;module.ntype=1;
		operation=2;make_mp=1;instance=H;
		if(mode==0)mp.ref=0;
		if(mode==1)mp.ref=-1;
		if(mode==2)mp.ref=0x7fffffff;
		if(mode==3)module.type=nil;
		if(mode==4)module.type=H;
		if(mode==5)moduletypes[0]=nil;
		if(mode==6)moduletypes[0]=H;
		if(mode==7)mp.size=-1;
		if(mode==8)mp.size=0x7fffffff;
		if(mode==9)module.origmp=nil;
		if(mode==10)mp.ref=0x7ffffffe;
		if(mode==11){mp.size=0;module.origmp=nil;}
		if(mode==12){moduletypes[0]=&Tmodlink;Tmodlink.ref=0x7ffffffe;}
		if(mode==13)mp.np=-1;
		if(mode==14)mp.np=2;
		if(mode==15){mp.np=1;mp.map[0]=0x01;}
		if(mode==16){mp.size=sizeof(void*)+1;mp.np=1;mp.map[0]=0xc0;}
		memmove(&beforetype,&mp,sizeof(mp));memmove(&beforemodlink,&Tmodlink,sizeof(Tmodlink));
		allocations=0;raised=nil;valid=mode==10 || mode==11;
		ok=kprotect(run,nil);
		if(ok!=valid || worker.nerr || module.ref!= (valid ? 2 : 1))abort();
		if(!ok){
			if(raised!=exHeap || allocations || live || instance!=H ||
			   memcmp(&mp,&beforetype,sizeof(mp)) || memcmp(&Tmodlink,&beforemodlink,sizeof(Tmodlink)))abort();
		}else{
			if(allocations!=2 || instance==H || mp.ref!=beforetype.ref+1)abort();
			if(mp.size && memcmp(instance->MP,payload,sizeof(source.bytes)))abort();
			destroy(instance);instance=H;
			if(live || module.ref!=1 || mp.ref!=beforetype.ref)abort();
		}
		if(memcmp(savedpayload,payload,sizeof(source.bytes)))abort();
		Tmodlink.ref=mp.ref=1;total++;
	}
	printf("PASS module-data retains: %lu descriptor/table/size/alias cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
