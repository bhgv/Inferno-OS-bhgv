/* Included in guiread.c: real directory/create/remove entry points. */
static void badcreate(Chan *c,char *name,char *expected) {
    if(waserror()) {assert(strcmp(last_error,expected)==0);balanced();return;}
    guicreate(c,name,OREAD,0);assert(0);
}
static void badremove(Chan *c,char *expected) {
    if(waserror()) {assert(strcmp(last_error,expected)==0);balanced();return;}
    guiremove(c);assert(0);
}
static void test_guilifecycle(void) {
    Chan c={0},root={0};
    Cname cname={0};
    Dir d;
    int i,successes=0,failures=0,calls;
    static char longname[2048];
    lifecycle=1;method.name="frame";method.opts=nooptions;
    getter_value="";getter_throw=command_error=drop_top=missing_process=0;
    widget_name=".";memset(&widget,0,sizeof(widget));memset(&child,0,sizeof(child));
    widget.type=TKframe;widget.guiqid=1;top.root=&widget;process.tktop=&top;
    process.pid=100000;process.osenv=&environment;environment.user="owner";
    c.qid.path=QPID(process.pid)|1;c.qid.vers=process.pid;c.qid.type=QTDIR;
    assert(guigen(&root,nil,nil,0,DEVDOTDOT,&d)==1 && d.qid.path==0);balanced();
    assert(guigen(&root,"100000",nil,0,0,&d)==1 && d.qid.path==QPID(100000));balanced();
    assert(guigen(&root,nil,nil,0,0,&d)==1 && d.qid.path==QPID(100000));balanced();
    assert(guigen(&root,"4294967296",nil,0,0,&d)==-1);balanced();
    assert(guigen(&c,nil,nil,0,-10,&d)==-1);balanced();
    assert(guigen(&c,".self",nil,0,0,&d)==1 && d.qid.path==(c.qid.path|QCTL_NAME));balanced();
    assert(qid2tk(&c.qid,nil)==nil && tk2id(nil,&widget)==-1);
    badcreate(&root,"a.frame",Eperm);badcreate(&c,".",Efilename);
    badcreate(&c,"a.unknown",Eperm);badcreate(&c,"frame",Eperm);badcreate(&c,".frame",Eperm);
    memset(longname,'n',sizeof(longname)-1);longname[sizeof(longname)-1]=0;
    badcreate(&c,longname,Etoobig);
    missing_process=1;badcreate(&c,"made.frame",Ethread);missing_process=0;
    process.tktop=nil;remote_owner=1;calls=display_calls;
    badcreate(&c,"made.frame",Eperm);assert(display_calls==calls);
    remote_owner=0;badcreate(&c,"made.frame",Eio);assert(display_calls==calls+1);
    process.tktop=&top;
    /* A failed allocation at any formatter/command boundary must unwind. */
    for(i=0;i<100;i++) {
        Chan made=c;
        fail_after=i;
        if(waserror()) {
            assert(strcmp(last_error,Enomem)==0);balanced();failures++;
        }else{
            guicreate(&made,"made.frame",ORDWR,DMDIR);poperror();successes++;
            assert(QID(made.qid)==child.guiqid && PATH(made.qid)==QPID(100000));
            assert(made.qid.type==QTDIR && made.mode==ORDWR && (made.flag&COPEN));
        }
        fail_after=-1;balanced();assert(live_allocations==0);
    }
    assert(successes && failures);
    /* Lookup must accept immutable names and preserve the complete high PID. */
    assert(guigen(&c,"made.frame",nil,0,0,&d)==1 && d.qid.path==(QPID(100000)|child.guiqid));balanced();
    assert(guigen(&c,nil,nil,0,1,&d)==1 && strcmp(d.name,"made.frame")==0);balanced();
    child.name_tail=longname;
    if(waserror()){assert(strcmp(last_error,Etoobig)==0);balanced();}
    else {guigen(&c,nil,nil,0,1,&d);assert(0);}
    child.name_tail=child_tail;
    command_error=1;badcreate(&c,"made.frame",Ebadarg);command_error=0;
    getter_throw=1;badcreate(&c,"made.frame",Ebadarg);getter_throw=0;
    cname.s="made.frame";c.name=&cname;c.qid.path=QPID(100000)|child.guiqid;c.qid.type=0;
    root.name=&cname;badremove(&root,Eperm);
    cname.s="..";badremove(&c,Efilename);cname.s="made.frame";
    missing_process=1;badremove(&c,Ethread);missing_process=0;
    process.tktop=nil;badremove(&c,Enonexist);process.tktop=&top;
    command_error=1;badremove(&c,Ebadarg);command_error=0;
    getter_throw=1;badremove(&c,Ebadarg);getter_throw=0;
    /* Remove commands must retain a widget path longer than the old 100 bytes. */
    strcpy(child_path,longname);
    for(i=0;i<60;i++) {
        fail_after=i;
        if(waserror()){assert(strcmp(last_error,Enomem)==0);balanced();}
        else {guiremove(&c);poperror();assert(strlen(last_widget_command)>2048);}
        fail_after=-1;balanced();assert(live_allocations==0);
    }
    drop_top=1;guiremove(&c);drop_top=0;process.tktop=&top;balanced();
    assert(live_allocations==0);
    /* Qids survive sibling deletion/reordering and never follow reused memory. */
    {
        Tk a={0},b={0},replacement={0},*fresh;
        TkTop alternate={0};
        Qid qa=c.qid,qb=c.qid,qroot=c.qid;
        uint saved;
        int aid,bid;
        poison_alloc=1;fresh=tknewobj(&top,TKframe,sizeof(Tk));poison_alloc=0;
        assert(fresh && fresh->guiqid==0);free(fresh);
        widget.siblings=&a;a.siblings=&b;
        aid=tk2id(&top,&a);bid=tk2id(&top,&b);
        qa.path=QPID(100000)|aid;qb.path=QPID(100000)|bid;
        assert(qid2tk(&qa,&top)==&a && qid2tk(&qb,&top)==&b);
        widget.siblings=&b;assert(qid2tk(&qa,&top)==nil && qid2tk(&qb,&top)==&b);
        b.siblings=&widget;top.root=&b;widget.siblings=nil;
        assert(qid2tk(&qb,&top)==&b);
        a.guiqid=0;a.siblings=nil;widget.siblings=&a;
        assert(tk2id(&top,&a)!=aid && qid2tk(&qa,&top)==nil);
        alternate.root=&replacement;tk2id(&alternate,&replacement);
        assert(qid2tk(&qb,&alternate)==nil);
        qroot.path=QPID(100000)|tk2id(&top,&widget)|QCTL_NAME;
        assert(qid2tk(&qroot,&alternate)==nil && qid2tk(&qroot,&top)==&widget);
        saved=guinextid;guinextid=0x7fffffffUL;
        replacement.guiqid=0;
        if(waserror()){assert(strcmp(last_error,Etoobig)==0);balanced();}
        else {tk2id(&alternate,&replacement);assert(0);}
        assert(replacement.guiqid==0 && guinextid==0x7fffffffUL);
        guinextid=saved;top.root=&widget;widget.siblings=nil;widget.slave=nil;
    }
    lifecycle=0;
    puts("PASS GUI directory/create/remove: high PID Qids, immutable lookup, type validation, allocation and Tk failures, long paths and lock balance");
}
