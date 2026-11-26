Awmfiles: module
{
	PATH: con "/dis/wm/awmfiles.dis";

	init: fn(ctxt: ref Draw->Context, argv: list of string);
	choose: fn(ctxt: ref Draw->Context, directory: string): string;
};
