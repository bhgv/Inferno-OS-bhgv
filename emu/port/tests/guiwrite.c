/* Included by guiread.c: exercise production write parsing and ownership. */
static void badwrite(Chan *c,void *data,long count,vlong offset,char *expected) {
    if(waserror()) {
        assert(strcmp(last_error,expected)==0);balanced();return;
    }
    guiwrite(c,data,count,offset);assert(0);
}
static void test_guiwrite(Chan *c) {
    const char *command="configure -text hello";
    char *exact, many[300], out[16000], snapshot[16000], expected[16000];
    static char payload[1024],name[2048];
    long n;
    int i,calls,failures=0,successes=0;
    c->qid.type=0;c->flag=COPEN;top.root=&widget;process.tktop=&top;
    widget.type=TKframe;method.name="frame";method.opts=nooptions;
    widget_name=".widget";getter_value="ok";
    exact=malloc(strlen(command));assert(exact);memcpy(exact,command,strlen(command));
    assert(guiwrite(c,exact,strlen(command),0)==(long)strlen(command));
    assert(memcmp(exact,command,strlen(command))==0);free(exact);
    assert(strcmp(last_widget_command,".widget configure -text hello")==0);
    assert(guiread(c,out,sizeof(out),0)==3 && memcmp(out,"ok\n",3)==0);guiclose(c);balanced();
    command="\"configure -text hello\"\n";
    assert(guiwrite(c,(void*)command,strlen(command),7)==(long)strlen(command));
    assert(strcmp(last_widget_command,".widget configure -text hello")==0);guiclose(c);
    command="configure -text 'raw \"quotes\" \\path";
    assert(guiwrite(c,(void*)command,strlen(command),1)==(long)strlen(command));
    snprintf(expected,sizeof(expected),".widget %s",command);
    assert(strcmp(last_widget_command,expected)==0);guiclose(c);
    command="\"configure -text 'a\\\"b\\\\c\nline\"\n";
    assert(guiwrite(c,(void*)command,strlen(command),1)==(long)strlen(command));
    assert(strcmp(last_widget_command,".widget configure -text 'a\"b\\c\nline")==0);guiclose(c);
    calls=getter_calls;
    badwrite(c,(void*)"\"unterminated",13,0,Ebadarg);
    badwrite(c,(void*)"\"x\"junk",7,0,Ebadarg);
    badwrite(c,(void*)"x\0y",3,0,Ebadarg);
    assert(getter_calls==calls);
    assert(guiwrite(c,(void*)"\"\"\n# comment\n",13,0)==13 && getter_calls==calls);
    assert(guiwrite(c,(void*)"",0,0)==0);
    badwrite(c,(void*)"x",-1,0,Ebadarg);badwrite(c,(void*)"x",1,-1,Ebadarg);
    badwrite(c,(void*)"x",0x7fffffffL,0,Ebadarg);
    for(i=0;i<100;i++){many[i*2]='x';many[i*2+1]='\n';}
    calls=getter_calls;
    assert(guiwrite(c,many,198,0)==198 && getter_calls==calls+100); /* 99 + update */
    assert(guiread(c,out,sizeof(out),0)==99*3);guiclose(c);
    calls=getter_calls;badwrite(c,many,200,0,Etoobig);
    assert(getter_calls==calls && !root_rslt_strings && live_allocations==0);
    /* Responses and command writes are streams; a positive file offset is valid. */
    getter_value="first";assert(guiwrite(c,(void*)"x",1,10)==1);
    getter_value="second";assert(guiwrite(c,(void*)"x",1,11)==1);
    assert(guiread(c,out,sizeof(out),12)==6 && memcmp(out,"first\n",6)==0);
    assert(guiread(c,out,sizeof(out),18)==7 && memcmp(out,"second\n",7)==0);guiclose(c);
    /* Snapshot envelope round-trip, including literal quotes/backslashes/UTF-8. */
    widget.type=TKtext;method.name="text";method.opts=nil;
    getter_value="\316\261 \"b\" \\path\nline";
    n=guiread(c,snapshot,sizeof(snapshot),0);assert(n>0);
    assert(guiwrite(c,snapshot,n,0)==n);
    snprintf(expected,sizeof(expected),".widget insert end '%s",getter_value);
    assert(strcmp(last_widget_command,expected)==0);
    assert(((GuiRead*)c->aux)->data==nil);guiclose(c);assert(live_allocations==0);
    /* Tk failures after producing owned results must release everything. */
    command_error=1;badwrite(c,(void*)"x",1,0,Ebadarg);command_error=0;
    getter_throw=1;badwrite(c,(void*)"x",1,0,Ebadarg);getter_throw=0;
    drop_top=1;badwrite(c,(void*)"x\ny",3,0,Enonexist);drop_top=0;process.tktop=&top;
    assert(!root_rslt_strings && live_allocations==0);balanced();
    memset(payload,'P',sizeof(payload)-1);payload[sizeof(payload)-1]=0;
    memset(name,'N',sizeof(name)-1);name[0]='.';name[sizeof(name)-1]=0;
    getter_value=payload;widget_name=name;
    for(i=0;i<100;i++) {
        fail_after=i;
        if(waserror()) {
            assert(strcmp(last_error,Enomem)==0);balanced();failures++;
        }else{
            assert(guiwrite(c,(void*)"a\nb\nc",5,0)==5);poperror();successes++;
        }
        fail_after=-1;guiclose(c);balanced();assert(live_allocations==0 && !root_rslt_strings);
    }
    assert(failures>0 && successes>0);
    missing_process=1;badwrite(c,(void*)"x",1,0,Ethread);missing_process=0;
    top.root=nil;assert(guiwrite(c,(void*)"x",1,0)==0);top.root=&widget;
    c->qid.type=QTDIR;badwrite(c,(void*)"x",1,0,Eisdir);c->qid.type=0;
    guiclose(c);balanced();assert(live_allocations==0);
    puts("PASS GUI write: immutable/bounded input, strict command parsing, escaped snapshot round-trip, 99-command boundary, stream offsets and failure cleanup");
}
