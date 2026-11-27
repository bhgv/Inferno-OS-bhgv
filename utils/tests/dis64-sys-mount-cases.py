"""Source and exact native-boundary events for selected guest mount composition."""

def programs():
    dial='(status,c):=sys->dial("one",nil);if(status<0)return;'
    mount='r:=sys->mount(c.dfd,nil,"/mounted",Sys->MREPL,"");if(r<0)return;'
    unmount='r=sys->unmount(nil,"/mounted");'
    events=[('dial','one',0,0,0,''),('mount','/mounted',100,0,0,'')]
    end=[('unmount','',0,0,0,'/mounted')]
    opening='fd:=sys->open("/mounted/data",Sys->OREAD);b:=array[16]of byte;r=sys->read(fd,b,len b);if(r<0)return;text:=string b[:r];r=sys->werrstr(text);'
    reads=[('open','/mounted/data',0,0,150),('read','data',150,16,4),('werrstr','data',0,0,0)]
    return [
        ('mount-only',dial+mount+unmount,events+end,0,False),
        ('mount-open',dial+mount+opening+unmount,events+reads+end,0,False),
        ('mount-failure',dial+mount+unmount,events[:1]+[('mount','/mounted',100,0,-1,'')],0,False),
        ('mount-unicode',dial+mount.replace('/mounted','/Grüße').replace(',"")',',"世界")')+unmount.replace('/mounted','/Grüße'),[events[0],('mount','/Grüße',100,0,0,'世界'),('unmount','',0,0,0,'/Grüße')],0,False),
        ('mount-reopen',dial+mount+opening+'fd=sys->open("/mounted/data",Sys->OREAD);b=array[16]of byte;r=sys->read(fd,b,len b);'+unmount,events+reads+[('open','/mounted/data',0,0,150),('read','data',150,16,4)]+end,0,False),
        ('mount-open-failure',dial+mount+'fd:=sys->open("/mounted/missing",Sys->OREAD);b:=array[16]of byte;r=sys->read(fd,b,len b);'+unmount,events+[('open','/mounted/missing',0,0,-1),('read','',-1,16,-1)]+end,0,False),
        ('mount-remount',dial+mount+unmount+mount.replace('r:=','r=')+unmount,events+end+[events[1]]+end,0,False),
        ('mount-origin',dial.replace('if(status<0)return;','')+mount.replace('if(r<0)return;','')+opening+unmount,events+reads+end,0,False),
        ('mount-flags-word',dial+'r:=status;r=sys->mount(c.dfd,nil,"/mounted",r,"");'+unmount,events+end,0,False),
        ('mount-create',dial+mount+'fd:=sys->create("/mounted/new",Sys->ORDWR,8r640);b:=array of byte "new";r=sys->write(fd,b,len b);'+unmount,events+[('create','/mounted/new',2,0o640,151),('write','new',151,3,3)]+end,0,False),
        ('mount-create-failure',dial+mount+'fd:=sys->create("/mounted/new",Sys->OWRITE,8r600);b:=array[8]of byte;r=sys->read(fd,b,len b);'+unmount,events+[('create','/mounted/new',1,0o600,-1),('read','',-1,8,-1)]+end,0,False),
        ('mount-remove',dial+mount+'r=sys->remove("/mounted/old");'+unmount,events+[('remove','/mounted/old',0,0,-1)]+end,0,False),
        ('mount-chdir',dial+mount+'r=sys->chdir("/mounted");r=sys->chdir("/");'+unmount,events+[('chdir','/mounted',0,0,-1),('chdir','/',0,0,-1)]+end,0,False),
        ('mount-bind',dial+mount+'r=sys->bind("/mounted","/other",Sys->MBEFORE);r=sys->unmount(nil,"/other");'+unmount,events+[('bind','/mounted',1,0,0,'/other'),('unmount','',0,0,0,'/other')]+end,0,False),
        ('mount-twelve-imports',dial+mount+'fd:=sys->create("/mounted/new",Sys->ORDWR,8r640);b:=array of byte "new";r=sys->write(fd,b,len b);fd=sys->open("/mounted/data",0);b=array[16]of byte;r=sys->read(fd,b,len b);r=sys->iounit(fd);r=sys->werrstr("done");r=sys->bind("/mounted","/other",0);r=sys->chdir("/ok");r=sys->remove("/victim");'+unmount,events+[('create','/mounted/new',2,0o640,151),('write','new',151,3,3),('open','/mounted/data',0,0,150),('read','data',150,16,4),('iounit','',150,0,4096),('werrstr','done',0,0,0),('bind','/mounted',0,0,0,'/other'),('chdir','/ok',0,0,0),('remove','/victim',0,0,0)]+end,0,False),
    ]
