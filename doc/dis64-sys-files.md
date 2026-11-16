# Create and remove files through native64 Styx

The selected Limbo profile composes `dial/mount/create/open/read/write/remove/chdir/bind/unmount` through the existing native Sys implementations. Create needs `MCREATE` on the mount and write permission on the server root. This example uses only synthetic per-connection data in `tools/libstyx/tests/loopback.c`; it does not write host files. Prepare `/mnt` and `/alternate` in the emu root, and allow the emu username in the fixture.

```limbo
implement SysText;
include "sys.m";
SysText: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    (status, c) := sys->dial("tcp!127.0.0.1!5640", nil);
    if(status < 0) return;
    r := sys->mount(c.dfd, nil, "/mnt", Sys->MCREATE, "");
    if(r < 0) return;
    r = sys->chdir("/mnt");
    fd := sys->create("created", Sys->ORDWR, 8r640);
    buffer := array of byte "created-data";
    r = sys->write(fd, buffer, len buffer);
    r = sys->bind("/mnt", "/alternate", Sys->MREPL);
    fd = sys->open("/alternate/created", Sys->OREAD);
    r = sys->remove("created");
    buffer = array[32] of byte;
    r = sys->read(fd, buffer, len buffer);
    if(r < 0) return;
    text := string buffer[:r];
    r = sys->werrstr(text);
    r = sys->chdir("/");
    r = sys->unmount(nil, "/alternate");
    r = sys->unmount(nil, "/mnt");
}
```

Compile with `limbo -64l -I module -o files.dis64 files.b`. Put the output inside the emu root, then run `emu-g -r /path/to/root -c 1 -d /files.dis64 -P '$Sys' -X dis64-managed-trial`. Use `-c 0` for the C interpreter. The fixture's port replaces 5640.

`chdir` resolves relative paths in the existing namespace. `bind` makes the same mounted tree visible under another target. Replacing `fd` retires the previous FD owner. Removing the name prevents subsequent walks; the already open channel still reads `created-data`. `werrstr` records that text in the error buffer; it does not print it.

Native create passes mode at byte72 and permission at byte76 in the generated 80-byte C frame. Its returned FD uses the same separate mapped caller root as open; scalar return values cannot overlap that root. The serialized private signature tree comes from the existing Sys interface; no new public wire kind or Styx message format is introduced.

This is a bounded acceptance example: one separate FD result, replacement, linear calls and early return guards, at most32 calls and12 selected Sys methods. It omits general error cleanup, repeated reads and independent FD aliases, which the selected profile does not yet admit. It is tested on x64 and ARM64/QEMU in C/JIT with direct and fragmented TCP replies. Android GUI/AWM startup remains unproven.
