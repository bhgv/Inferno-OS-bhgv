/* Bounded compiler proof for complete Sys dial calls with an unused local tuple. */
static int
dis64dialshape(Decl *entry,Decl *globals,Decl *ldts,Inst *input,long count,long databytes,Dis64ManagedCompilePlan *plan)
{
    Decl *method,*field,*connection,*g,*path=nil,*nilvalue=nil,*empty=nil,*literals[Dis64ManagedLiterals];
    Inst *in;
    long module=-1,frame=-1,result=-1;
    unsigned int i,n=0,phase=0,calls=0,j,returned=0;
    uvlong seen=0;
    if(entry==nil || entry->pc==nil || entry->pc->pc!=0 || entry->ty->ids || entry->ty->tof->kind!=Tnone || entry->ty->varargs ||
       entry->desc==nil || count<7 || count>12*Dis64ManagedCalls+1 || input==nil || input->op!=ILOAD ||
       ldts==nil || ldts->next || ldts->init==nil || ldts->init->val!=1 || ldts->init->decl==nil)return 0;
    method=ldts->init->decl->ty->ids;
    if(method==nil || method->next || method->dot==nil || strcmp(method->dot->sym->name,"Sys") ||
       strcmp(method->sym->name,"dial") || method->ty->kind!=Tfn || method->ty->varargs || method->ty->polys || method->ty->tof->kind!=Ttuple)return 0;
    field=method->ty->ids;
    for(i=0;i<2;i++,field=field->next)if(field==nil || field->ty->kind!=Tstring)return 0;
    if(field)return 0;
    field=method->ty->tof->ids;
    if(field==nil || field->ty->kind!=Tint || field->next==nil || field->next->next || field->next->ty->kind!=Tadt)return 0;
    connection=field->next->ty->ids;
    if(connection==nil || !dis64sysfdtype(connection->ty) || connection->next==nil || !dis64sysfdtype(connection->next->ty) ||
       connection->next->next==nil || connection->next->next->ty->kind!=Tstring || connection->next->next->next)return 0;
    for(g=impdecl->ty->tof->ids;g;g=g->next)if(g->store==Dglobal || (g->store==Dfn && g->sym!=entry->sym))return 0;
    for(g=globals;g;g=g->next){
        if(g->store!=Dglobal)continue;
        if(g->offset<0 || g->offset%4 || g->offset>=databytes || g->offset/4>=3+Dis64ManagedLiterals || (seen&((uvlong)1<<(g->offset/4))))return 0;
        seen |= (uvlong)1<<(g->offset/4);
        if(g->ty->kind==Tany && g->init==nil && strcmp(g->sym->name,"nil")==0){if(nilvalue)return 0;nilvalue=g;continue;}
        if(g->ty->kind==Tstring && g->init==nil && g->sym->len==0){if(empty)return 0;empty=g;continue;}
        if(g->ty->kind!=Tstring || g->init==nil || g->init->op!=Oconst || g->init->decl==nil)return 0;
        if(g->offset==input->s.reg){if(path || g->init->decl->sym->len!=4 || strcmp(g->init->decl->sym->name,"$Sys"))return 0;path=g;}
        else{if(n==Dis64ManagedLiterals)return 0;literals[n]=g;plan->literals[n++]=g->init->decl->sym;}
    }
    if(!path || databytes!=4*(1+!!nilvalue+!!empty+n))return 0;
    for(in=input,i=0;in;in=in->next){
        if(in->op==INOOP)continue;
        if(i++>=(unsigned long)count)return 0;
        if(in->op==IRET){if(in->next || phase!=1 || !calls || in->sm!=Anone || in->dm!=Anone || in->mm!=Anone)return 0;returned=1;continue;}
        if(in->op==ILOAD){
            if((phase!=0 && phase!=1) || calls==Dis64ManagedCalls || plan->reloads[calls] || in->sm!=Amp || in->dm!=Afp || in->mm!=Aldt || in->s.reg!=path->offset || in->m.decl!=ldts || !dis64clockslot(in->d.reg,entry->desc->size))return 0;
            module=in->d.reg;plan->reloads[calls]=1;phase=1;continue;
        }
        if(phase==1){
            if(in->op!=IMFRAME || in->sm!=Afp || in->dm!=Afp || in->mm!=Aoff || in->s.reg!=module || in->m.decl==nil || in->m.decl->iface!=method ||
               !dis64clockslot(in->d.reg,entry->desc->size) || in->d.reg==module || calls==Dis64ManagedCalls)return 0;
            frame=in->d.reg;
        }else if(phase==2 || phase==3){
            if(in->op!=IMOVP || in->sm!=Amp || in->dm!=Afpind || in->mm!=Anone || in->d.reg!=frame || in->d.offset!=(phase==2?32:36))return 0;
            j=2;
            if(!((nilvalue && in->s.reg==nilvalue->offset)||(empty && in->s.reg==empty->offset))){
                for(j=0;j<n && literals[j]->offset!=in->s.reg;j++);if(j==n)return 0;j+=3;
            }
            plan->pairtransfers[calls][phase-2]=j;
        }else if(phase==4){
            if(in->op!=ILEA || in->sm!=Afp || in->dm!=Afpind || in->mm!=Anone || in->d.reg!=frame || in->d.offset!=16 ||
               (result>=0 && in->s.reg!=result) || in->s.reg<32 || in->s.reg%4 || in->s.reg>entry->desc->size-16 || (module>=in->s.reg && module<in->s.reg+16) || (frame>=in->s.reg && frame<in->s.reg+16))return 0;
            result=in->s.reg;
        }else if(phase==5){
            if(in->op!=IMCALL || in->sm!=Afp || in->dm!=Afp || in->mm!=Aoff || in->s.reg!=frame || in->d.reg!=module || in->m.decl==nil || in->m.decl->iface!=method)return 0;
            calls++;phase=1;continue;
        }else return 0;
        phase++;
    }
    if(i!=(unsigned long)count || phase!=1 || !calls || !returned || (calls<Dis64ManagedCalls && plan->reloads[calls]))return 0;
    for(j=0;j<n;j++){for(i=0;i<calls && plan->pairtransfers[i][0]!=j+3 && plan->pairtransfers[i][1]!=j+3;i++);if(i==calls)return 0;}
    plan->module="$Sys";plan->path="$Sys";plan->symbols[0]="dial";plan->bindings=1;plan->calls=calls;plan->nliterals=n;return 1;
}

static int
dis64dialemit(Dis64ManagedCompilePlan *plan,uchar *code,unsigned int *bytes)
{
    unsigned int i,j,at=4,count=0;
    Dis64Instruction in;
    for(i=0;i<plan->calls;i++){
        if(plan->reloads[i]){in=(Dis64Instruction){ILOAD,SRC(AMP)|AXIMM|DST(AMP),0,16,0};if(!dis64encode(code,&at,&in))return 0;count++;}
        for(j=0;j<2;j++){
            unsigned int transfer=plan->pairtransfers[i][j];
            in=(Dis64Instruction){IMOVP,SRC(AMP)|DST(AFP),0,transfer==2?8:24+8*(transfer-3),64+8*j};if(!dis64encode(code,&at,&in))return 0;
        }
        in=(Dis64Instruction){IMFRAME,SRC(AMP)|AXIMM|DST(AFP),0,0,40};if(!dis64encode(code,&at,&in))return 0;
        for(j=0;j<2;j++){
            code[at++]=IMOVP;code[at++]=SRC(AFP)|DST(AFP|AIND);at+=disoperand(code+at,4,64+8*j);code[at++]=40;at+=disoperand(code+at,4,64+8*j);
        }
        code[at++]=ILEA;code[at++]=SRC(AFP)|DST(AFP|AIND);at+=disoperand(code+at,4,80);code[at++]=40;code[at++]=32;
        in=(Dis64Instruction){IMCALL,SRC(AFP)|AXIMM|DST(AMP),0,40,0};if(!dis64encode(code,&at,&in))return 0;
        count+=7;
    }
    code[at++]=IEXIT;code[at++]=SRC(AXXX)|DST(AXXX);dis64put32(code,count+1);*bytes=at;return 1;
}
