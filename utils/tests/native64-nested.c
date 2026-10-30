/* Appended to the actual collector and heap fixture; exercise allocating descendants. */


/* Clone three nested arrays while every descendant allocation runs actual collector epochs. */
int
main(void)
{
	Type *type;
	F_Roots_copy *frame;
	Array *outer, *middle, *bytes, *copy, *copymiddle, *copybytes;
	int phase, state, cases, bucket;

	cases = 0;
	for(phase = 0; phase < 3; phase++){
		for(state = Palt; state <= Pbroken; state++){
			startcase(phase, state);
			type = native64frametype(&limbo_native64_Roots_copy_entry);
			check(type != nil);
			frame = (F_Roots_copy*)stackframes(type, 1);
			outer = H2D(Array*, heaparray(&Tptr, 1));
			frame->source = outer;
			middle = H2D(Array*, heaparray(&Tptr, 1));
			memmove(outer->data, &middle, sizeof(middle));
			bytes = H2D(Array*, heaparray(&Tbyte, 3));
			memmove(middle->data, &bytes, sizeof(bytes));
			memmove(bytes->data, "abc", 3);
			allocationcollections = 1;
			copy = arraycpy(outer);
			allocationcollections = 0;
			for(bucket = 0; bucket < PTRHASH; bucket++)
				check(ptrtab[bucket] == nil);
			check(!freed[2] && !freed[3] && !freed[4]);
			check(!freed[5] && !freed[6] && !freed[7]);
			check(copy != outer && D2H(copy)->ref == 1);
			memmove(&copymiddle, copy->data, sizeof(copymiddle));
			memmove(&copybytes, copymiddle->data, sizeof(copybytes));
			check(copymiddle != middle && copybytes != bytes);
			check(copybytes->len == 3 && memcmp(copybytes->data, "abc", 3) == 0);
			copybytes->data[0] = 'z';
			check(bytes->data[0] == 'a');
			destroy(copy);
			check(freed[5] && freed[6] && freed[7]);
			freeptrs(frame, type);
			initmem(type, frame);
			check(freed[2] && freed[3] && freed[4]);
			process.R.EX = process.R.SP = nil;
			epochs(6);
			check(Tarray.ref == 1 && Tptr.ref == 1 && Tbyte.ref == 1);
			freetype(type);
			cases++;
		}
	}
	printf("PASS nested native64 arrays: %d predicates, %d allocating deep clones, actual heap/collector\n", checks, cases);
	return 0;
}
