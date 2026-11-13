#ifndef DIS64_SYS_CONNECTION_H
#define DIS64_SYS_CONNECTION_H

static const char *const dis64connectionnames[]={"dial","read","write","iounit","werrstr"};
static int
dis64connectionkind(const DisStringPoolView *pool,unsigned int symbol)
{
    unsigned int i;
    for(i=0;i<5;i++)if(dis64sysstringname(pool,symbol,dis64connectionnames[i]))return i==0?14:dis64sysfixedkind(dis64connectionnames[i]);
    return -1;
}
static long
dis64connectioncell(long cell)
{
    return cell==80?64:cell==128?72:cell==132?68:cell==136?76:-1;
}

static int
dis64sysconnectionview(const Dis64StorageImage *image,Dis64ManagedLiteralsView *literals)
{
    Dis64Type frame,byte;DisRuneView name;Dis64CallDecoded decoded;Dis64Instruction in,flow[Dis64ClockInstructions];
    unsigned int i,j,at=4,used,phase=0,loaded=0,calls=0,seen=0,ordinal=0,parameter=0,staged=0,imports=image->imports.count;
    int kinds[5],kind,hasdial=0;long source,middle,dest;
    if(image->module.profile!=1 || image->module.entry || image->module.name!=2 || image->module.frametype || image->module.datatype!=1 ||
       image->types.count!=3 || image->exports.count || !imports || imports>5 || image->initializers.count || image->data.align!=8 ||
       !dis64typeget(&image->types,0,&frame) || frame.bytes!=144 || frame.align!=8 || frame.mapbytes!=3 || frame.map[0] || frame.map[1]!=0xdf || frame.map[2] ||
       !dis64typeget(&image->types,2,&byte) || byte.bytes!=1 || byte.align!=1 || byte.mapbytes!=1 || byte.map[0] ||
       !dis64sysstringname(&image->strings,3,"$Sys") || !disstringpoolget(&image->strings,2,&name) || !name.count || name.count>255 ||
       image->code.count>Dis64ClockInstructions || image->strings.count!=3+imports+(image->data.bytes-24)/8)return 0;
    for(i=0;i<name.count;i++){unsigned int rune;if(!disruneat(&name,i,&rune)||!rune||rune>127)return 0;}
    for(i=0;i<imports;i++){kinds[i]=dis64connectionkind(&image->strings,i?3+i:1);if(kinds[i]<0)return 0;if(kinds[i]==14)hasdial=1;}
    if(!hasdial)return 0;
    for(i=0;i<image->code.count;i++){
        if(!dis64callread(image->code.data+at,image->code.bytes-at,&decoded,&used))return 0;
        at+=used;in=decoded.instruction;flow[i]=in;
        if(!phase && in.op==ILOAD){
            if(in.mode!=(SRC(AMP)|AXIMM|DST(AMP)) || in.source!=16 || in.middle || in.destination)return 0;loaded=1;continue;
        }
        if(!phase && in.op==IMFRAME){
            if(!loaded || in.mode!=(SRC(AMP)|AXIMM|DST(AFP)) || in.source || in.middle<0 || (unsigned long)in.middle>=imports || in.destination!=40)return 0;
            ordinal=in.middle;parameter=staged=0;phase=1;seen|=1U<<ordinal;continue;
        }
        if(phase==1){
            unsigned int argc=kinds[ordinal]==14?2:dis64sysfixedargs(kinds[ordinal]);
            kind=kinds[ordinal];
            if(parameter<argc){
                if(kind==14 || kind==0){
                    dest=kind==14?64+8*parameter:64;
                    if(!staged){
                        if(in.op!=IMOVP || UDST(in.mode)!=AFP || (in.mode&ARM)!=AXNON || in.destination!=dest)return 0;
                        if(USRC(in.mode)==AMP){
                            if(in.source!=8 && (in.source<24 || in.source%8 || (unsigned long)in.source>=image->data.bytes))return 0;
                            flow[i]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,0,40};
                        }else if(USRC(in.mode)==AFP && (in.source==104 || in.source==120))flow[i]=(Dis64Instruction){IMOVW,SRC(AFP)|DST(AFP|AIND),0,in.source==104?96:120,40};
                        else return 0;
                        staged=1;continue;
                    }
                    if(in.op!=IMOVP || in.mode!=(SRC(AFP)|DST(AFP|AIND)) || in.source!=dest || in.destination!=40 || decoded.secondary!=dest)return 0;
                    flow[i]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,0,40};
                }else if(kind==9 || parameter<2){
                    if(!staged && in.op==IMOVP && in.mode==(SRC(AMP)|DST(AFP)) && in.source==8 && in.destination==64){
                        flow[i]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,0,40};staged=1;continue;
                    }
                    if(in.op!=IMOVP || in.mode!=(SRC(AFP)|DST(AFP|AIND)) || in.destination!=40 || decoded.secondary!=64+8*parameter)return 0;
                    if(in.source==64 && staged)source=0;
                    else if(!parameter && (in.source==88 || in.source==96))source=96;
                    else if(parameter==1 && in.source==112)source=112;
                    else return 0;
                    flow[i]=(Dis64Instruction){IMOVW,SRC(source?AFP:AIMM)|DST(AFP|AIND),0,source,40};
                }else{
                    if(in.destination!=40 || decoded.secondary!=80 || UDST(in.mode)!=(AFP|AIND) || (in.mode&ARM)!=AXNON)return 0;
                    if(in.op==ILENA){if(USRC(in.mode)!=AFP || in.source!=112)return 0;source=112;}
                    else if(in.op==IMOVW){source=in.source;if(USRC(in.mode)==AFP){source=dis64connectioncell(source);if(source<0)return 0;}else if(USRC(in.mode)!=AIMM)return 0;}
                    else return 0;
                    flow[i]=(Dis64Instruction){IMOVW,SRC(USRC(in.mode))|DST(AFP|AIND),0,source,40};
                }
                staged=0;parameter++;continue;
            }
            if(staged || in.op!=ILEA || in.mode!=(SRC(AFP)|DST(AFP|AIND)) || in.source!=(kind==14?80:128) || in.destination!=40 || decoded.secondary!=32)return 0;
            phase=2;continue;
        }
        if(phase==2){
            if(in.op!=IMCALL || in.mode!=(SRC(AFP)|AXIMM|DST(AMP)) || in.source!=40 || in.middle!=ordinal || in.destination || ++calls>Dis64ManagedCalls)return 0;
            if(kinds[ordinal]==14)flow[i].middle=-2;phase=0;continue;
        }
        if(in.op==IEXIT){if(in.mode!=(SRC(AXXX)|DST(AXXX)))return 0;continue;}
        if(dis64clockbranch(in.op)){
            if(in.op==IJMP || UDST(in.mode)!=AIMM)return 0;
            source=in.source;middle=in.middle;
            if(USRC(in.mode)==AFP){source=dis64connectioncell(source);if(source<0)return 0;}else if(USRC(in.mode)!=AIMM)return 0;
            if((in.mode&ARM)==AXINF){middle=dis64connectioncell(middle);if(middle<0)return 0;}else if((in.mode&ARM)!=AXIMM)return 0;
            flow[i].source=source;flow[i].middle=middle;continue;
        }
        if(dis64clockwordop(in.op)){
            if(UDST(in.mode)!=AFP)return 0;dest=dis64connectioncell(in.destination);if(dest<0)return 0;
            source=in.source;middle=in.middle;
            if(USRC(in.mode)==AFP){source=dis64connectioncell(source);if(source<0)return 0;}else if(USRC(in.mode)!=AIMM)return 0;
            if(in.op!=IMOVW){if((in.mode&ARM)==AXINF){middle=dis64connectioncell(middle);if(middle<0)return 0;}else if((in.mode&ARM)!=AXIMM)return 0;}
            else if((in.mode&ARM)!=AXNON)return 0;
            flow[i].source=source;flow[i].middle=middle;flow[i].destination=dest;continue;
        }
        if(in.op==IMOVP && in.mode==(SRC(AMP)|DST(AFP)) && in.source==8 && (in.destination==112 || in.destination==120)){
            flow[i]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP),0,0,in.destination};continue;
        }
        if(in.op==ISLICEA){
            if(UDST(in.mode)!=AFP || in.destination!=112)return 0;
            if(USRC(in.mode)==AFP){flow[i].source=dis64connectioncell(in.source);if(flow[i].source<0)return 0;}else if(USRC(in.mode)!=AIMM)return 0;
            if((in.mode&ARM)==AXINF){flow[i].middle=dis64connectioncell(in.middle);if(flow[i].middle<0)return 0;}else if((in.mode&ARM)!=AXIMM)return 0;
            continue;
        }
        if(in.op==ICVTCA || in.op==ICVTAC || in.op==ILENA || in.op==INEWA || in.op==INEWAZ){
            unsigned int sm=USRC(in.mode);source=in.source;
            if(UDST(in.mode)!=AFP)return 0;
            if(in.op==INEWA || in.op==INEWAZ){
                if(in.destination!=112 || in.middle!=2 || (in.mode&ARM)!=AXIMM)return 0;
                if(sm==AFP){source=dis64connectioncell(source);if(source<0)return 0;}else if(sm!=AIMM)return 0;dest=112;
            }else{
                if((in.mode&ARM)!=AXNON)return 0;
                if(in.op==ICVTCA){
                    if(in.destination!=112)return 0;
                    if(sm==AMP){if(source!=8 && (source<24 || source%8 || (unsigned long)source>=image->data.bytes))return 0;source=0;sm=AIMM;}
                    else if(sm==AFP && source==104)source=96;
                    else if(sm!=AFP || source!=120)return 0;dest=112;
                }else{
                    if(sm!=AFP || source!=112)return 0;
                    if(in.op==ICVTAC){if(in.destination!=120)return 0;dest=120;}
                    else{dest=dis64connectioncell(in.destination);if(dest<0)return 0;}
                }
            }
            flow[i]=(Dis64Instruction){IMOVW,SRC(sm)|DST(AFP),0,source,dest};continue;
        }
        return 0;
    }
    if(phase || !calls || at!=image->code.bytes || seen!=(1U<<imports)-1)return 0;
    for(i=0;i<image->code.count;i++)if(dis64clockbranch(flow[i].op)){
        if(i+2>=image->code.count || flow[i].destination!=i+2 || flow[i+1].op!=IEXIT)return 0;
    }
    (void)j;
    return dis64clockflow(flow,image->code.count) && dis64managedliteralview(image,literals);
}
#endif
