# Compile with limbo -6; run in the experimental native64 scalar console.
# This entry has no imports, arguments, result pointer or managed objects.
implement Native64Probe;

Native64Probe: module {
	init: fn(); # Sole executable entry; this is not a shell command ABI.
};

counter: int;


# Check actual arithmetic and backedges across several scheduler quanta.
init()
{
	counter = 0;
	for(i := 0; i < 4097; i++)
		counter++;
	if(counter != 4097)
		counter /= 0;
}
