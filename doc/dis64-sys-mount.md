# Mount Styx from Limbo on native64

The selected Dis64 profile now executes ordinary `Sys.dial`, `Sys.mount`, `Sys.open`, `Sys.read`, `Sys.write` and `Sys.unmount` calls. This client expects a libstyx fixture exposing `first` at `tcp!127.0.0.1!5640`. The emu root must contain `/mnt`. The server must allow the emu user; this is not an authentication example.

Save as `mountdemo.b`.

```limbo
implement SysText;
include "sys.m";
SysText: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    (status, c) := sys->dial("tcp!127.0.0.1!5640", nil);
    if(status < 0) return;
    r := sys->mount(c.dfd, nil, "/mnt", Sys->MREPL, "");
    if(r < 0) return;
    fd := sys->open("/mnt/first", Sys->OREAD);
    buffer := array[64] of byte;
    r = sys->read(fd, buffer, len buffer);
    if(r < 0) return;
    text := string buffer[:r];
    r = sys->werrstr(text);
    r = sys->unmount(nil, "/mnt");
}
```

```sh
limbo -64l -I module -o mountdemo.dis64 mountdemo.b
emu-g -r /path/to/root -c 1 -d /mountdemo.dis64 -P '$Sys' -X dis64-managed-trial
```

The output file must be inside the selected emu root. Use `-c 0` for the C interpreter. Only the actual returned byte count is converted; `werrstr` places `text` in the error buffer for this example’s diagnostics. It does not print to the console. A general client needs repeated reads through EOF and error cleanup; loops and such handlers remain outside the selected profile.

`load Sys Sys->PATH` compiles to LOAD with path `$Sys`. The interface hash and C frame come from `sys.m` and Limbo generation. Mount receives the transport FD, nil auth-FD, target, flags and spec. The wire protocol remains base 9P2000; native pointers and ADTs are not transmitted.

An open FD retains its channel after unmount; replacement or program retirement closes it. The current profile allows one separate open result, including replacement, alongside Connection fields. GC tracks the FD root separately from numeric results. Linear calls and early returns, up to32 calls and8 selected Sys methods, are supported. Multiple independent FDs, arbitrary ADTs/aliases, loops and general sh/AWM remain separate work.

The tutorial compiles and executes in C/JIT on x64 and ARM64/QEMU over real TCP, including fragmented version replies. This establishes guest mount in emu, not Android GUI/APK startup.
