implement Native64Procedure;

Native64Procedure: module {
	init: fn(); # Sole exported entry.
};

a := 12345;
b := 789;
result: int;
tally: int;


# Renew the child's declared local on every call.
bump(amount: int)
{
	local: int;
	local++;
	tally += local*amount;
}


# Preserve a parent local across both its loop and a nested child call.
step()
{
	local: int;
	for(i := 0; i < 3; i++)
		local++;
	bump(1);
	if(local != 3)
		tally /= 0;
}


# Keep expression temporaries private even when this helper is called from another helper.
fold(x: int, y: int, wide: big, fraction: real)
{
	local: int;
	local++;
	if(wide != big 17 || fraction != 1.25 || local != 1)
		result /= 0;
	result = ((x^y)&65535)|65536;
}


# Keep a private numeric case table around the nested scalar call.
mix()
{
	if(a == 0)
		return;
	case b {
	0 => result /= 0;
	700 to 800 => fold(a, b, big 17, 1.25);
	* => result /= 0;
	}
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
