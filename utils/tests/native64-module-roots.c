/* Appended to the complete collector fixture; linkmod comes from its complete owning source. */
static Module constructingmodule; // Source module owns its original data until the final release.
static int constructing; // An unload during construction exposes an unrooted partial instance.
static int unloads; // Actual freemodlink calls reaching the module-owner boundary.


/* Account for the real instance destructor without importing unrelated module cache services. */
void
unload(Module *module)
{
	check(!constructing && module == &constructingmodule && module->ref == 2);
	module->ref--;
	unloads++;
}


/* Dynamic initialization is outside this ordinary typed-MP construction gate. */
void
newdyndata(Modlink *module)
{
	USED(module);
	check(0);
}


/* No import failure is expected from the empty sentinel-terminated table. */
void
kwerrstr(char *format, ...)
{
	USED(format);
	check(0);
}


/* Keep unexpected link diagnostics distinct from a successful construction path. */
int
snprint(char *buffer, int length, char *format, ...)
{
	USED(buffer);
	USED(length);
	USED(format);
	check(0);
	return 0;
}


/* Construct module data with nested arrays while each native allocation completes collector epochs. */
int
main(void)
{
	Type *mpkind, *callertype, *types[1];
	Import imports[1];
	Modlink *instance;
	F_Roots_caller *caller;
	Array *array, *bytes, *copy, *copybytes;
	void *original, *empty;
	uchar map[] = {0x80};
	int phase, state, cases, bucket, cached;
	char *selection;

	cases = 0;
	selection = getenv("NATIVE64_MODULE_CASE");
	empty = H;
	for(cached = 1; cached >= 0; cached--){
		if(selection != nil && ((cached && strcmp(selection, "unpublished") == 0) ||
		   (!cached && strcmp(selection, "cached") == 0)))
			continue;
		for(phase = 0; phase < 3; phase++){
			for(state = Palt; state <= Pbroken; state++){
				startcase(phase, state);
				callertype = native64frametype(&limbo_native64_Roots_caller_entry);
				check(callertype != nil);
				caller = (F_Roots_caller*)stackframes(callertype, 1);
				mpkind = dtype(freeheap, sizeof(void*), map, sizeof(map));
				check(mpkind != nil);
				original = H2D(void*, heapz(mpkind));
				array = H2D(Array*, heaparray(&Tptr, 1));
				memmove(original, &array, sizeof(array));
				bytes = H2D(Array*, heaparray(&Tbyte, 3));
				memmove(array->data, &bytes, sizeof(bytes));
				memmove(bytes->data, "abc", 3);
				memset(&constructingmodule, 0, sizeof(constructingmodule));
				memset(imports, 0, sizeof(imports));
				types[0] = mpkind;
				constructingmodule.origmp = original;
				constructingmodule.type = types;
				constructingmodule.ntype = 1;
				constructingmodule.ref = 2;
				modules = cached ? &constructingmodule : nil;
				constructing = 1;
				allocationcollections = 1;
				instance = linkmod(&constructingmodule, imports, 1);
				allocationcollections = 0;
				constructing = 0;
				modules = &constructingmodule;
				check(instance != H && instance->m == &constructingmodule);
				memmove(&caller->result, &instance, sizeof(instance));
				for(bucket = 0; bucket < PTRHASH; bucket++)
					check(ptrtab[bucket] == nil);
				epochs(6);
				check(!freed[2] && !freed[3] && !freed[4] && !freed[5]);
				check(!freed[6] && !freed[7] && !freed[8]);
				memmove(&copy, instance->MP, sizeof(copy));
				memmove(&copybytes, copy->data, sizeof(copybytes));
				check(copy != array && copybytes != bytes && instance->MP != original);
				check(copybytes->len == 3 && memcmp(copybytes->data, "abc", 3) == 0);
				copybytes->data[0] = 'z';
				check(bytes->data[0] == 'a');
				memmove(&caller->result, &empty, sizeof(empty));
				destroy(instance);
				check(freed[5] && freed[6] && freed[7] && freed[8]);
				check(constructingmodule.ref == 1 && Tmodlink.ref == 1);
				modules = nil;
				constructingmodule.origmp = H;
				destroy(original);
				process.R.EX = process.R.SP = nil;
				epochs(6);
				check(freed[2] && freed[3] && freed[4]);
				check(mpkind->ref == 1 && Tarray.ref == 1 && Tptr.ref == 1 && Tbyte.ref == 1);
				freetype(mpkind);
				freetype(callertype);
				cases++;
			}
		}
	}
	check(unloads == cases);
	printf("PASS module construction roots: %d predicates, %d instances, actual link/heap/collector\n", checks, cases);
	return 0;
}
