implement Driver;

include "sys.m";
include "draw.m";
include "sum.m";

Driver: module
{
	init: fn(nil: ref Draw->Context, nil: list of string);
};

# This caller can stay interpreted while the Sum module uses generated C.
init(nil: ref Draw->Context, nil: list of string)
{
	sys := load Sys Sys->PATH;
	math := load Sum Sum->PATH;
	if(math == nil){
		sys->fprint(sys->fildes(2), "cannot load sum.dis: %r\n");
		return;
	}
	sys->print("sum(10) = %d\n", math->sum(10));
}
