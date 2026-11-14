/* Bounded scalar DAG callers. No new wire IDs or caller-supplied proof facts. */
#ifndef DIS64_CALL_CHAIN_H
#define DIS64_CALL_CHAIN_H
#include "dis64-call-infer.h"

static int
dis64chainbody(const Dis64LocalFunction *functions, const Dis64LocalCallee *callees,
	unsigned int functionsused,unsigned int owner, unsigned int framebytes, const Dis64FunctionLocals *locals,
	const Dis64CallDecoded *decoded, Dis64InitWork *scratch, Dis64BoundCall *result)
{
	Dis64LocalPlan plan;
	Dis64LocalState state = {0};
	Dis64LocalEvent event, zeroevent = {0};
	Dis64LocalOperands fact, zerofact = {0};
	const Dis64Instruction *instruction, *call;
	const Dis64LocalCallee *callee;
	unsigned int pc, j, chosen, pointer, sm, dm, kind, op, index, id, end;

	plan.framebytes = framebytes;
	plan.argumentend = functions[owner].argumentend;
	plan.resultcell = functions[owner].resultcell;
	plan.result = functions[owner].signature.result;
	plan.callees = functionsused;
	plan.functions = callees;
	if(!dis64localplanvalid(&plan))
		return 0;
	end = functions[owner].end;
	pointer = 0;
	for(pc=functions[owner].entry; pc<end; pc++){
		instruction = &decoded[pc].instruction;
		sm = USRC(instruction->mode);
		dm = UDST(instruction->mode);
		event = zeroevent;
		fact = zerofact;
		fact.op = instruction->op;
		fact.edge = Dis64CFGNext;
		if(instruction->op == IFRAME){
			if(state.site != 0 || instruction->mode != (SRC(AIMM)|DST(AFP)) ||
			   (instruction->destination != 40 && instruction->destination != 48 && instruction->destination != 56))
				return 0;
			/* Disjoint islands are scanned once; nested FRAME or an early exit refuses. */
			for(j=pc+1; j<end; j++){
				op = decoded[j].instruction.op;
				if(op == ICALL || op == IFRAME || op == IEXIT)
					break;
			}
			if(j == end || decoded[j].instruction.op != ICALL)
				return 0;
			call = &decoded[j].instruction;
			chosen = functionsused;
			for(j=1; j<functionsused; j++)
				if(call->destination >= 0 && (unsigned long)call->destination == functions[j].entry)
					chosen = j;
			if(chosen == functionsused || instruction->source < 0 ||
			   (unsigned long)instruction->source != functions[chosen].frame)
				return 0;
			event.action = Dis64StatePrepare;
			event.site = pc+1;
			event.callee = chosen;
			event.frame = functions[chosen].frame;
			if(!dis64callstep(&plan, &state, &event))
				return 0;
			pointer = (unsigned int)instruction->destination;
		}else if(instruction->op == ICALL){
			if(state.site == 0 || instruction->mode != (SRC(AFP)|DST(AIMM)) ||
			   (unsigned long)instruction->source != pointer || instruction->destination < 0 ||
			   (unsigned long)instruction->destination != functions[state.callee-1].entry)
				return 0;
			event.action = Dis64StateCall;
			event.callee = state.callee-1;
			event.frame = functions[event.callee].frame;
			if(!dis64callstep(&plan, &state, &event))
				return 0;
			pointer = 0;
			result->calls++;
		}else if(state.site != 0 && dm == (AFP|AIND)){
			callee = callees+state.callee-1;
			if((unsigned long)instruction->destination != pointer)
				return 0;
			if(instruction->op == ILEA){
				if(instruction->mode != (SRC(AFP)|DST(AFP|AIND)) ||
				   instruction->source < 0 || (unsigned long)instruction->source != plan.resultcell ||
				   decoded[pc].secondary != 32)
					return 0;
				event.action = Dis64StateBind;
				event.kind = callee->result;
				event.source = plan.resultcell;
				event.destination = 32;
				event.width = 8;
				if(!dis64callstep(&plan, &state, &event))
					return 0;
				result->bindings++;
			}else{
				if(!dis64callargumentindex(callee, decoded[pc].secondary, &index))
					return 0;
				kind = callee->kinds[index];
				op = kind == Dis64Big ? IMOVL : kind == Dis64Real ? IMOVF : IMOVW;
				if(instruction->op != op ||
				   !dis64localreadslot(locals->slots, locals->count, sm, instruction->source, kind, &fact.source))
					return 0;
				fact.kind = kind;
				fact.reads = fact.source != 0;
				event.action = Dis64StateArgument;
				event.index = index;
				event.kind = kind;
				if(!dis64callargument(&plan, &state, &event, sm, instruction->source))
					return 0;
				result->arguments++;
			}
		}else if(state.site != 0){
			return 0;
		}else if(dm==(AFP|AIND)){
            if(owner==0 || instruction->destination!=32 || decoded[pc].secondary!=0 ||
               (instruction->mode&ARM)!=AXNON || !dis64returnfact(instruction,plan.result,locals->slots,locals->count,&fact))return 0;
            event.action=Dis64StateStore;event.kind=plan.result;
            if(plan.result==Dis64Bool){if(sm!=AIMM)return 0;event.boolean=instruction->source;}
            if(!dis64callstep(&plan,&state,&event))return 0;
		}else if((instruction->op == IMOVW || instruction->op == IMOVL || instruction->op == IMOVF) &&
		   sm == AFP && instruction->source >= 0 && (unsigned long)instruction->source == plan.resultcell){
			kind = state.ready;
			op = kind == Dis64Big ? IMOVL : kind == Dis64Real ? IMOVF : IMOVW;
			if(kind == 0 || instruction->op != op || instruction->mode != (SRC(AFP)|DST(AFP)) ||
			   !dis64localreadslot(locals->slots, locals->count, AFP, instruction->destination, kind, &id))
				return 0;
			event.action = Dis64StateRead;
			event.kind = kind;
			if(!dis64callstep(&plan, &state, &event))
				return 0;
			fact.write = id;
			fact.kind = kind;
			result->results++;
		}else if(instruction->op == ICVTWF || instruction->op == ICVTWL){
			/* Existing int32 conversion supplies a typed Big/Real without inventing a seed. */
            kind = instruction->op == ICVTWL ? Dis64Big : Dis64Real;
			if((instruction->mode != (SRC(AIMM)|DST(AFP)) && instruction->mode != (SRC(AFP)|DST(AFP))) ||
			   !dis64localreadslot(locals->slots, locals->count, sm, instruction->source, Dis64Int, &fact.source) ||
			   !dis64localreadslot(locals->slots, locals->count, AFP, instruction->destination, kind, &id))
				return 0;
			fact.kind = kind;
			fact.write = id;
			fact.reads = fact.source != 0;
		}else if(instruction->op==IRET){
            if(owner==0 || instruction->mode!=(SRC(AXXX)|DST(AXXX)) || pc+1!=end)return 0;
            event.action=Dis64StateReturn;if(!dis64callstep(&plan,&state,&event))return 0;
            fact.edge=Dis64CFGTerminal;
		}else if(instruction->op == IEXIT){
			if(owner!=0 || instruction->mode != (SRC(AXXX)|DST(AXXX)) || pc+1 != end)
				return 0;
			fact.edge = Dis64CFGTerminal;
		}else{
			if(!dis64localoperandscheck(instruction, functions[owner].end, functions[owner].entry, end,
			   locals->slots, locals->count, framebytes, &fact) || fact.edge != Dis64CFGNext)
				return 0;
		}
		if(fact.edge == Dis64CFGNext && pc+1 == end)
			return 0;
		scratch[pc].fact = fact;
	}
	result->steps = end-functions[owner].entry;
	return 1;
}

/* Derive every edge from raw CALLs, then prove each function exactly once, callee
 * before caller. Scratch/work may change on refusal; publication remains atomic. */
static int
dis64chaincheck(Dis64CallInference *work,Dis64CallDecoded *decoded,Dis64InitWork *scratch,unsigned int *queue,
    unsigned int limit,Dis64BoundCall *output,unsigned int capacity,unsigned int *written)
{
    Dis64BoundCall results[Dis64LocalFunctions]={0};Dis64LocalCallee callees[Dis64LocalFunctions]={0};
    unsigned int edges[Dis64LocalFunctions][2]={{0}},done[Dis64LocalFunctions]={0},caller[Dis64LocalFunctions]={0};
    unsigned int seed[4],parameters,i,j,pc,n=work->count,used=0,spent=0,progress,target,first,end,store,kind;
    Dis64Instruction *in;Dis64LocalOperands fact;Dis64FunctionLocals *locals;Dis64LocalFunction *fn;
    if(!limit || limit>Dis64CallLimit || !n || n>capacity || n>Dis64LocalFunctions)return 0;
    for(i=0;i<n;i++){
        fn=work->functions+i;callees[i].frame=fn->frame;callees[i].result=fn->signature.result;callees[i].parameters=fn->signature.parameters;
        for(j=0;j<callees[i].parameters;j++)callees[i].kinds[j]=dis64u32(fn->signature.data+12+4*j);
        for(pc=fn->entry;pc<fn->end;pc++){
            in=&decoded[pc].instruction;
            if(in->op==IFRAME)caller[i]=1;
            if(in->op!=ICALL)continue;
            caller[i]=1;target=n;
            for(j=1;j<n;j++)if(in->destination>=0 && (unsigned long)in->destination==work->functions[j].entry)target=j;
            if(target==n)return 0;edges[i][target/32]|=1U<<(target%32);
        }
    }
    while(used<n){
        progress=0;
        for(i=0;i<n;i++){
            if(done[i])continue;
            for(j=0;j<n;j++)if((edges[i][j/32]&(1U<<(j%32))) && !done[j])break;
            if(j<n)continue;
            fn=work->functions+i;locals=work->locals+i;first=fn->entry;end=fn->end;kind=fn->signature.result;
            if(!dis64initseed(fn->signature.data,fn->signature.bytes,locals->slots,locals->count,seed,&parameters))return 0;
            if(i==0 || caller[i]){
                if(!dis64chainbody(work->functions,callees,n,i,work->frames[i],locals,decoded,scratch,results+i))return 0;
                if(results[i].steps>limit-spent)return 0;spent+=results[i].steps;
            }else for(pc=first;pc<end;pc++){
                in=&decoded[pc].instruction;store=UDST(in->mode)==(AFP|AIND);if(in->op==IEXIT)return 0;
                if(store){
                    if(in->destination!=32 || decoded[pc].secondary || (in->mode&ARM)!=AXNON ||
                       !dis64returnfact(in,kind,locals->slots,locals->count,&fact))return 0;
                }else if(in->op==IRET){
                    if(in->mode!=(SRC(AXXX)|DST(AXXX)))return 0;
                    fact=(Dis64LocalOperands){0};fact.op=IRET;fact.edge=Dis64CFGTerminal;
                }else{
                    if(!dis64localoperandscheck(in,work->functions[n-1].end,first,end,locals->slots,locals->count,work->frames[i],&fact))return 0;
                    if(fact.edge==Dis64CFGJump || fact.edge==Dis64CFGBranch)fact.target-=first;
                }
                if((fact.edge==Dis64CFGNext || fact.edge==Dis64CFGBranch) && pc+1==end)return 0;
                scratch[pc].fact=fact;
            }
            if(spent>=limit || !dis64initializedsolve(end-first,locals->count,parameters,seed,scratch+first,queue,limit-spent,&results[i].body.function.numeric))return 0;
            spent+=results[i].body.function.numeric.work;
            if(i && !dis64returncover(scratch+first,end-first,kind,queue,limit-spent,&results[i].body))return 0;
            spent+=results[i].body.work;
            results[i].body.function.symbol=fn->symbol;results[i].body.function.entry=first;results[i].body.function.end=end;results[i].body.function.frame=fn->frame;
            done[i]=1;used++;progress=1;
        }
        if(!progress)return 0; /* No fixed point for a self/mutual dependency cycle. */
    }
    for(i=0;i<n;i++)output[i]=results[i];*written=n;return 1;
}

/* Existing root/leaf records retain their exact old proof summaries. The DAG
 * extension independently rederives raw facts after an old-profile refusal. */
static int
dis64callchaininfercheck(const unsigned char *data,unsigned int bytes,unsigned int objectlimit,
    Dis64CallInference *work,Dis64CallDecoded *decoded,Dis64InitWork *scratch,unsigned int *queue,unsigned int workcapacity,
    unsigned int limit,Dis64BoundCall *output,unsigned int capacity,unsigned int *written,Dis64CallImage *result)
{
    Dis64CallImage image;
    if(!work || !decoded || !scratch || !queue || !output || !written || !result)return 0;
    if(dis64callinfercheck(data,bytes,objectlimit,work,decoded,scratch,queue,workcapacity,limit,output,capacity,written,result))return 1;
    if(!dis64callimageopen(data,bytes,objectlimit,&image) || !dis64callinferlocalsmode(&image,work,decoded,workcapacity,objectlimit,1) ||
       !dis64chaincheck(work,decoded,scratch,queue,limit,output,capacity,written))return 0;
    image.functions=*written;*result=image;return 1;
}
#endif
