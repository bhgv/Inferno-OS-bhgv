# Compile with limbo -6; this uses only the agreed scalar Dis64 wire profile.
implement Native64Case;

Native64Case: module {
	init: fn(); # Standalone scalar entry, without imports or managed arguments.
};

key := 17;
wide := big 16r7fffffffffffffff;
answer, tally: int;


# Check dense/sparse branches, loop backedges and the largest signed Big label.
init()
{
	for(i := 0; i < 4097; i++){
		case i%3 {
		0 => tally += 1;
		1 => tally += 2;
		2 => tally += 3;
		* => tally /= 0;
		}
	}
	case key {
	1 => answer = 11;
	10 to 20 => answer = 99;
	* => answer = 77;
	}
	if(tally != 8193 || answer != 99)
		answer /= 0;
	case wide {
	big -1 => answer = 1;
	big 16r7fffffffffffffff => answer = 5;
	* => answer = 6;
	}
	if(answer != 5)
		answer /= 0;
}
