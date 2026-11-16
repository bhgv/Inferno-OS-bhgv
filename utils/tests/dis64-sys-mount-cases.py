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
    ]
