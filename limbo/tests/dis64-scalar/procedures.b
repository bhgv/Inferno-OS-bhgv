implement Native64Procedure;

Native64Procedure: module {
	init: fn(); # Sole exported entry.
};

a := 12345;
b := 789;
result: int;
tally: int;


# Renew locals once per call; loop backedges must preserve the running sum.
step()
{
	local: int;
	for(i := 0; i < 3; i++)
		local++;
	tally += local-2;
}


# Exercise compiler temporaries in the helper's isolated storage region.
mix()
{
	if(a == 0)
		return;
	result = ((a^b)&65535)|65536;
}


# Keep caller locals live across repeated helpers and numeric case lowering.
init()
{
	x := big 17;
	y := 1.25;
	for(i := 0; i < 4097; i++)
		step();
	case tally {
	4097 => mix();
	* => result /= 0;
	}
	if(result != 78636 || tally != 4097 || x != big 17 || y != 1.25)
		result /= 0;
}
