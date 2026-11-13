/* A linear connection program may leave early through a checked Word guard. */
static long
dis64connectionword(long slot,long tuple,long *locals,int allocate)
{
    unsigned int i;
    if(tuple>=0 && slot==tuple)return 80;
    for(i=0;i<2;i++)if(locals[i]==slot)return 132+4*i;
    if(allocate)for(i=0;i<2;i++)if(locals[i]<0){locals[i]=slot;return 132+4*i;}
    return -1;
}

static int
dis64connectionemit(Dis64ManagedCompilePlan *p,unsigned int op,unsigned int mode,long middle,long source,long dest)
{
    if(p->connectioncount==Dis64ClockInstructions)return 0;
    p->connectioncode[p->connectioncount++]=(Dis64Instruction){op,mode,middle,source,dest};return 1;
}

static int
dis64connectionshape(Decl *entry,Decl *globals,Decl *ldts,Inst *input,long count,long databytes,Dis64ManagedCompilePlan *p)
{
    Decl *methods[5],*method,*field,*g,*path=nil,*nilvalue=nil,*empty=nil,*literals[Dis64ManagedLiterals];
    Inst *in,*scan;long module=-1,frame=-1,tuple=-1,result=-1,locals[2]={-1,-1};
    long array=-1,string=-1,temp=-1,pcs[12*Dis64ManagedCalls+1],target[Dis64ClockInstructions];
    long source,dest,middle,constant,kind=-1;unsigned int i,j,n=0,imports=0,phase=0,parameter=0,calls=0,ret=0,hasdial=0,extra=0,called=0;
    uvlong seen=0;
    if(entry==nil || entry->pc==nil || entry->pc->pc || entry->ty->ids || entry->ty->tof->kind!=Tnone || entry->ty->varargs ||
       entry->desc==nil || count<7 || count>12*Dis64ManagedCalls+1 || input==nil || input->op!=ILOAD ||
       ldts==nil || ldts->next || ldts->init==nil || ldts->init->val<1 || ldts->init->val>5 || ldts->init->decl==nil)return 0;
    for(method=ldts->init->decl->ty->ids;method;method=method->next){
        if(imports==5 || method->dot==nil || strcmp(method->dot->sym->name,"Sys") || method->ty->kind!=Tfn || method->ty->varargs || method->ty->polys)return 0;
        kind=!strcmp(method->sym->name,"dial")?14:dis64sysfixedkind(method->sym->name);
        if(kind!=14 && kind!=12 && kind!=13 && kind!=9 && kind!=0)return 0;
        for(j=0;j<imports;j++)if(p->nativekind[j]==kind)return 0;
        if(kind==14){
            field=method->ty->tof->ids;
            if(method->ty->tof->kind!=Ttuple || field==nil || field->ty->kind!=Tint || field->next==nil || field->next->next || field->next->ty->kind!=Tadt)return 0;
            field=field->next->ty->ids;
            if(field==nil || !dis64sysfdtype(field->ty) || field->next==nil || !dis64sysfdtype(field->next->ty) || field->next->next==nil || field->next->next->ty->kind!=Tstring || field->next->next->next)return 0;
            field=method->ty->ids;for(j=0;j<2;j++,field=field->next)if(field==nil || field->ty->kind!=Tstring)return 0;
            if(field)return 0;hasdial=1;
        }else{
            if(method->ty->tof->kind!=Tint)return 0;
            field=method->ty->ids;
            for(j=0;j<dis64sysfixedargs(kind);j++,field=field->next){
                if(field==nil)return 0;
                if((kind==9 || kind>=12) && j==0){if(!dis64sysfdtype(field->ty))return 0;}
                else if(kind>=12 && j==1){if(field->ty->kind!=Tarray || field->ty->tof->kind!=Tbyte)return 0;}
                else if(field->ty->kind!=(kind==0?Tstring:Tint))return 0;
            }
            if(field)return 0;
        }
        methods[imports]=method;p->nativekind[imports]=kind;p->symbols[imports++]=method->sym->name;
    }
    if(!hasdial || imports!=(unsigned long)ldts->init->val)return 0;
    for(scan=input;scan;scan=scan->next)if(scan->op!=ILOAD && scan->op!=IMFRAME && scan->op!=IMOVP && scan->op!=ILEA && scan->op!=IMCALL && scan->op!=IRET && scan->op!=INOOP)extra=1;
    if(imports==1 && !extra)return 0; /* Preserve the previous two-Type dial profile byte-for-byte. */
    for(g=impdecl->ty->tof->ids;g;g=g->next)if(g->store==Dglobal || (g->store==Dfn && g->sym!=entry->sym))return 0;
    for(g=globals;g;g=g->next){
        if(g->store!=Dglobal)continue;
        if(g->offset<0 || g->offset%4 || g->offset>=databytes || g->offset/4>=3+Dis64ManagedLiterals || (seen&((uvlong)1<<(g->offset/4))))return 0;
        seen|=(uvlong)1<<(g->offset/4);
        if(g->ty->kind==Tany && g->init==nil && !strcmp(g->sym->name,"nil")){if(nilvalue)return 0;nilvalue=g;continue;}
        if(g->ty->kind==Tstring && g->init==nil && !g->sym->len){if(empty)return 0;empty=g;continue;}
        if(g->ty->kind!=Tstring || g->init==nil || g->init->op!=Oconst || g->init->decl==nil)return 0;
        if(g->offset==input->s.reg){if(path || g->init->decl->sym->len!=4 || strcmp(g->init->decl->sym->name,"$Sys"))return 0;path=g;}
        else{if(n==Dis64ManagedLiterals)return 0;literals[n]=g;p->literals[n++]=g->init->decl->sym;}
    }
    if(!path || databytes!=4*(1+!!nilvalue+!!empty+n))return 0;
    for(i=0;i<nelem(pcs);i++)pcs[i]=-1;
    for(i=0;i<nelem(target);i++)target[i]=-1;
#define EMIT(o,m,x,s,d) if(!dis64connectionemit(p,o,m,x,s,d))return 0
#define WORD(m,a) ((m)==Aimm?(a).offset:(m)==Afp?dis64connectionword((a).reg,tuple,locals,0):-1)
    for(in=input,i=0;in;in=in->next){
        if(in->op==INOOP)continue;
        if(i++>=(unsigned long)count || in->pc<0 || in->pc>=count)return 0;
        pcs[in->pc]=p->connectioncount;
        if(in->op==ILOAD){
            if(phase || in->sm!=Amp || in->dm!=Afp || in->mm!=Aldt || in->m.decl!=ldts || in->s.reg!=path->offset || !dis64clockslot(in->d.reg,entry->desc->size))return 0;
            if(module>=0 && in->d.reg!=module)return 0;module=in->d.reg;
            EMIT(ILOAD,SRC(AMP)|AXIMM|DST(AMP),0,16,0);continue;
        }
        if(in->op==IMFRAME){
            if(phase || module<0 || calls==Dis64ManagedCalls || in->sm!=Afp || in->dm!=Afp || in->mm!=Aoff || in->s.reg!=module || !dis64clockslot(in->d.reg,entry->desc->size) || in->d.reg==module || in->m.decl==nil)return 0;
            for(j=0;j<imports && in->m.decl->iface!=methods[j];j++);if(j==imports)return 0;
            kind=p->nativekind[j];frame=in->d.reg;parameter=0;phase=1;
            EMIT(IMFRAME,SRC(AMP)|AXIMM|DST(AFP),j,0,40);continue;
        }
        if(phase==1){
            unsigned int argc=kind==14?2:dis64sysfixedargs(kind);
            if(parameter<argc){
                long offset=32+4*parameter;
                if(in->dm!=Afpind || in->d.reg!=frame || in->d.offset!=offset || in->mm!=Anone)return 0;
                if(kind==14 || kind==0){
                    if(in->op!=IMOVP || (in->sm!=Amp && in->sm!=Afp))return 0;
                    if(in->sm==Amp){
                        constant=dis64managedconstant(nilvalue,empty,literals,n,in->s.reg);
                        if(constant==Dis64ManagedNil)source=8;
                        else if(constant<=Dis64ManagedLiteral && constant>Dis64ManagedLiteral-(long)n)source=24+8*(Dis64ManagedLiteral-constant);
                        else return 0;
                    }else if(in->s.reg==string)source=120;
                    else if(tuple>=0 && in->s.reg==tuple+12)source=104;
                    else return 0;
                    dest=kind==14?64+8*parameter:64;
                    EMIT(IMOVP,SRC(in->sm==Amp?AMP:AFP)|DST(AFP),0,source,dest);
                    EMIT(IMOVP,SRC(AFP)|DST(AFP|AIND),dest,dest,40);
                }else if(kind==9 || parameter<2){
                    if(in->op!=IMOVP || (in->sm!=Afp && in->sm!=Amp))return 0;
                    if(in->sm==Amp){if(dis64managedconstant(nilvalue,empty,literals,n,in->s.reg)!=Dis64ManagedNil)return 0;source=64;EMIT(IMOVP,SRC(AMP)|DST(AFP),0,8,64);}
                    else if(parameter==0){if(tuple<0 || (in->s.reg!=tuple+4 && in->s.reg!=tuple+8))return 0;source=in->s.reg==tuple+4?88:96;}
                    else{if(in->s.reg!=array)return 0;source=112;}
                    EMIT(IMOVP,SRC(AFP)|DST(AFP|AIND),64+8*parameter,source,40);
                }else if(in->op==ILENA){
                    if(in->sm!=Afp || in->s.reg!=array)return 0;
                    EMIT(ILENA,SRC(AFP)|DST(AFP|AIND),80,112,40);
                }else{
                    if(in->op!=IMOVW || (in->sm!=Aimm && in->sm!=Afp))return 0;
                    source=WORD(in->sm,in->s);if(in->sm==Afp && source<0)return 0;
                    EMIT(IMOVW,SRC(in->sm==Aimm?AIMM:AFP)|DST(AFP|AIND),80,source,40);
                }
                parameter++;continue;
            }
            if(in->op!=ILEA || in->sm!=Afp || in->dm!=Afpind || in->mm!=Anone || in->d.reg!=frame || in->d.offset!=16)return 0;
            if(kind==14){if(in->s.reg<32 || in->s.reg>entry->desc->size-16 || in->s.reg%4 || (tuple>=0 && tuple!=in->s.reg))return 0;result=in->s.reg;}
            else{
                if(in->s.reg<32 || in->s.reg%4 || in->s.reg>entry->desc->size-4 || in->s.reg==module || in->s.reg==frame || (tuple>=0 && in->s.reg>=tuple && in->s.reg<tuple+16))return 0;
                result=dis64connectionword(in->s.reg,tuple,locals,1);if(result<0 || result==80)return 0;
            }
            EMIT(ILEA,SRC(AFP)|DST(AFP|AIND),32,kind==14?80:128,40);phase=2;continue;
        }
        if(phase==2){
            if(in->op!=IMCALL || in->sm!=Afp || in->dm!=Afp || in->mm!=Aoff || in->s.reg!=frame || in->d.reg!=module || in->m.decl==nil)return 0;
            for(j=0;j<imports && in->m.decl->iface!=methods[j];j++);if(j==imports || p->nativekind[j]!=kind)return 0;
            EMIT(IMCALL,SRC(AFP)|AXIMM|DST(AMP),j,40,0);
            if(kind==14)tuple=result;else{EMIT(IMOVW,SRC(AFP)|DST(AFP),0,128,result);}
            called|=1U<<j;calls++;phase=0;continue;
        }
        if(in->op==IRET){
            if(in->sm!=Anone || in->dm!=Anone || in->mm!=Anone)return 0;
            if(in->next==nil)ret=1;EMIT(IEXIT,SRC(AXXX)|DST(AXXX),0,0,0);continue;
        }
        if(dis64clockbranch(in->op)){
            if(in->op==IJMP || in->branch==nil || in->dm!=Apc || in->next==nil || in->next->op!=IRET || in->branch->pc!=in->next->pc+1)return 0;
            source=WORD(in->sm,in->s);middle=WORD(in->mm,in->m);
            if((in->sm!=Aimm && (in->sm!=Afp || source<0)) || (in->mm!=Aimm && (in->mm!=Afp || middle<0)))return 0;
            target[p->connectioncount]=in->branch->pc;
            EMIT(in->op,SRC(in->sm==Aimm?AIMM:AFP)|(in->mm==Aimm?AXIMM:AXINF)|DST(AIMM),middle,source,0);continue;
        }
        if(in->op==ICVTCA || in->op==ICVTAC || in->op==ILENA || in->op==INEWA || in->op==INEWAZ || in->op==ISLICEA){
            if(in->dm!=Afp || !dis64clockslot(in->d.reg,entry->desc->size) || in->d.reg==module)return 0;
            if(in->op==INEWA || in->op==INEWAZ){
                if(in->mm!=Adesc || in->m.decl==nil || in->m.decl->ty==nil || in->m.decl->ty->kind!=Tbyte || in->m.decl->desc==nil || in->m.decl->desc->size!=1 || !dis64nomap(in->m.decl->desc))return 0;
                source=WORD(in->sm,in->s);if(in->sm!=Aimm && (in->sm!=Afp || source<0))return 0;
                EMIT(in->op,SRC(in->sm==Aimm?AIMM:AFP)|AXIMM|DST(AFP),2,source,112);array=in->d.reg;temp=-1;continue;
            }
            if(in->op==ISLICEA){
                if(in->d.reg!=array && in->d.reg!=temp)return 0;
                source=WORD(in->sm,in->s);middle=WORD(in->mm,in->m);
                if((in->sm!=Aimm && (in->sm!=Afp || source<0)) || (in->mm!=Aimm && (in->mm!=Afp || middle<0)))return 0;
                EMIT(ISLICEA,SRC(in->sm==Aimm?AIMM:AFP)|(in->mm==Aimm?AXIMM:AXINF)|DST(AFP),middle,source,112);array=in->d.reg;temp=-1;continue;
            }
            if(in->mm!=Anone || (in->sm!=Amp && in->sm!=Afp))return 0;
            if(in->op==ICVTCA){
                if(in->sm==Amp){constant=dis64managedconstant(nilvalue,empty,literals,n,in->s.reg);if(constant==Dis64ManagedNil)source=8;else if(constant<=Dis64ManagedLiteral && constant>Dis64ManagedLiteral-(long)n)source=24+8*(Dis64ManagedLiteral-constant);else return 0;}
                else if(in->s.reg==string)source=120;else if(tuple>=0 && in->s.reg==tuple+12)source=104;else return 0;
                EMIT(ICVTCA,SRC(in->sm==Amp?AMP:AFP)|DST(AFP),0,source,112);array=in->d.reg;temp=-1;
            }else{
                if(in->sm!=Afp || in->s.reg!=array)return 0;
                if(in->op==ICVTAC){EMIT(ICVTAC,SRC(AFP)|DST(AFP),0,112,120);string=in->d.reg;}
                else{dest=dis64connectionword(in->d.reg,tuple,locals,1);if(dest<0)return 0;EMIT(ILENA,SRC(AFP)|DST(AFP),0,112,dest);}
            }
            continue;
        }
        if(in->op==IMOVP && in->sm==Afp && in->dm==Afp && in->mm==Anone && in->s.reg==array && in->next && in->next->op==ISLICEA && in->next->d.reg==in->d.reg){temp=in->d.reg;continue;}
        if(in->op==IMOVP && in->sm==Amp && in->dm==Afp && in->mm==Anone && dis64managedconstant(nilvalue,empty,literals,n,in->s.reg)==Dis64ManagedNil){
            if(in->d.reg==array){EMIT(IMOVP,SRC(AMP)|DST(AFP),0,8,112);array=-1;continue;}
            if(in->d.reg==string){EMIT(IMOVP,SRC(AMP)|DST(AFP),0,8,120);string=-1;continue;}
        }
        if(dis64clockwordop(in->op)){
            if(in->dm!=Afp)return 0;source=WORD(in->sm,in->s);dest=dis64connectionword(in->d.reg,tuple,locals,1);
            if(dest<0 || (in->sm!=Aimm && (in->sm!=Afp || source<0)))return 0;
            middle=0;
            if(in->op!=IMOVW){middle=in->mm==Anone?dest:WORD(in->mm,in->m);if(in->mm!=Aimm && in->mm!=Anone && (in->mm!=Afp || middle<0))return 0;}
            EMIT(in->op,SRC(in->sm==Aimm?AIMM:AFP)|(in->op==IMOVW?0:in->mm==Aimm?AXIMM:AXINF)|DST(AFP),middle,source,dest);continue;
        }
        return 0;
    }
#undef WORD
#undef EMIT
    if(i!=(unsigned long)count || phase || !calls || !ret || called!=(1U<<imports)-1)return 0;
    for(i=0;i<p->connectioncount;i++)if(target[i]>=0){if(target[i]>=count || pcs[target[i]]<0)return 0;p->connectioncode[i].destination=pcs[target[i]];}
    p->module="$Sys";p->path="$Sys";p->bindings=imports;p->calls=calls;p->nliterals=n;p->arrayroots=1;return 1;
}
