#include <stdio.h>
#include <string.h>
#include "dis-layout.h"


/* Report the failed invariant without disabling checks in release builds. */
static int
failure(unsigned int line)
{
	fprintf(stderr, "FAIL layout fixture at line %u\n", line);
	return 1;
}

#define CHECK(condition) do { if(!(condition)) return failure(__LINE__); } while(0) // Always-on fixture check.


/* Check every single-bit map through two bitmap bytes against a byte oracle. */
static int
maps(DisAbi *abi, unsigned int *cases)
{
	unsigned int size, bytes, bit, offset;
	unsigned char map[2];

	for(size=0; size<=abi->mapquantum*16; size++){
		CHECK(disabmapbytes(abi, size, &bytes));
		CHECK(bytes == (size/(abi->mapquantum*8)+(size%(abi->mapquantum*8) != 0)));
		memset(map, 0, sizeof(map));
		CHECK(disabmap(abi, size, map, bytes));
		CHECK(!disabmap(abi, size, map, bytes+1));
		if(bytes != 0)
			CHECK(!disabmap(abi, size, map, bytes-1));
		for(bit=0; bit<bytes*8; bit++){
			memset(map, 0, sizeof(map));
			map[bit/8] = 0x80U>>(bit%8);
			offset = bit*abi->mapquantum;
			CHECK(disabmap(abi, size, map, bytes) ==
				(offset <= size && size-offset >= abi->ptrbytes));
			(*cases)++;
		}
	}
	return 0;
}


/* Exercise both targets, mixed/nested records, arrays and atomic rejection. */
int
main(void)
{
	DisAbi abi, bad;
	DisLayout layout, saved, nested;
	DisFrameLayout frame;
	unsigned int p, offset, size, stride, i, cases, bytes;
	unsigned char map;

	cases = 0;
	for(p=4; p<=8; p+=4){
		CHECK(disabinit(&abi, p));
		CHECK(disabvalid(&abi));
		layout.next = 0;
		layout.align = 1;
		CHECK(disabfield(&abi, &layout, DisWord, &offset) && offset == 0);
		CHECK(disabfield(&abi, &layout, DisPointer, &offset) && offset == p);
		CHECK(disabfield(&abi, &layout, DisWord, &offset) && offset == p*2);
		CHECK(disabfinish(&layout, &size) && size == p*3);
		map = 0x40;
		CHECK(disabmap(&abi, size, &map, 1));
		CHECK(disabframes(&abi, &frame));
		CHECK(frame.lr == 0 && frame.fp == p && frame.mr == 2*p && frame.type == 3*p);
		CHECK(frame.result == 4*p && frame.stemp == 5*p && frame.rtemp == 6*p);
		CHECK(frame.dtemp == 7*p && frame.args == 8*p && disabframe(&abi) == 8*p);
		nested = layout;
		CHECK(disabarray(size, 3, &stride) && stride == size*3);
		CHECK(disabplace(&layout, stride, nested.align, &offset) && offset == size);
		CHECK(disabfinish(&layout, &stride) && stride == size*4);
		layout.next = 0;
		layout.align = 1;
		for(i=0; i<100; i++)
			CHECK(disabfield(&abi, &layout, DisWord, &offset) && offset == i*4);
		CHECK(disabfinish(&layout, &size) && size == 400 && layout.align == 4);
		CHECK(disabarray(4, 7, &size) && size == 28);
		layout.next = DisMaxOffset-1;
		layout.align = 1;
		saved = layout;
		offset = 123;
		CHECK(!disabfield(&abi, &layout, DisPointer, &offset));
		CHECK(memcmp(&layout, &saved, sizeof(layout)) == 0 && offset == 123);
		CHECK(!disabplace(&layout, 1, 3, &offset));
		layout.next = 4;
		layout.align = 3;
		CHECK(!disabfinish(&layout, &size));
		CHECK(!disabfield(&abi, &layout, DisByte, &offset));
		CHECK(disabsize(&abi, DisByte) == 1 && disabsize(&abi, DisShort) == 2);
		CHECK(disabsize(&abi, DisBig) == 8 && disabsize(&abi, DisReal) == 8);
		CHECK(disabsize(&abi, -1) == 0);
		CHECK(disabmapbytes(&abi, DisMaxOffset, &bytes));
		CHECK(bytes == (unsigned int)DisMaxOffset/(p*8)+1);
		CHECK(maps(&abi, &cases) == 0);
		CHECK(disabmap(&abi, 0, 0, 0));
		CHECK(!disabmap(&abi, p, 0, 1));
	}
	size = 99;
	CHECK(!disabarray(8, DisMaxOffset/8+1, &size) && size == 99);
	CHECK(disabarray(0, 0xffffffffU, &size) && size == 0);
	CHECK(!disabinit(&abi, 3));
	bad.ptrbytes = 8;
	bad.mapquantum = 4;
	CHECK(!disabvalid(&bad) && disabsize(&bad, DisPointer) == 0 && disabframe(&bad) == 0);
	CHECK(!disabmapbytes(&bad, 4, &bytes));
	CHECK(!disabinit(0, 8) && !disabframes(&abi, 0));
	printf("PASS ABI32/map4 and ABI64/map8 layout, nested records, frame, arrays; %u single-bit map fixtures\n", cases);
	return 0;
}
