implement StyxFrameSize;
include "sys.m";
include "draw.m";
include "styx.m";

styx: Styx;                 # Loaded codec module used by the regression.
Tmsg, Rmsg: import styx;     # Bind request and response methods to the actual module.


# Ordinary Inferno command entry; the context and argument list are deliberately unused.
StyxFrameSize: module
{
    init: fn(context: ref Draw->Context, arguments: list of string);
};


# Exercise real request/reply decoders, never a rewritten model of their size checks.
# context/arguments are borrowed and unused; return normally only after all checks pass.
init(nil: ref Draw->Context, nil: list of string)
{
    sys := load Sys Sys->PATH;
    styx = load Styx Styx->PATH;
    if (sys == nil || styx == nil) {
        raise "fail: Styx frame test modules";
    }
    styx->init();
    header := array[] of {byte 0, byte 0, byte 0, byte 0, byte 0, byte 0, byte 0};
    for (size := 0; size < 7; size++) {
        header[0] = byte size;
        (used, request) := styx->Tmsg.unpack(header);
        (replyused, reply) := styx->Rmsg.unpack(header);
        if (used != -1 || request != nil || replyused != -1 || reply != nil) {
            raise "fail: impossible Styx frame size";
        }
    }
    for (length := 0; length < 7; length++) {
        (used, request) := styx->Tmsg.unpack(header[:length]);
        (replyused, reply) := styx->Rmsg.unpack(header[:length]);
        if (used != 0 || request != nil || replyused != 0 || reply != nil) {
            raise "fail: incomplete Styx header";
        }
    }
    # Real Tread: tag 1, fid 7, offset 4294967300 and count 16, all in wire order.
    wire := array[] of {byte 23, byte 0, byte 0, byte 0, byte 116, byte 1, byte 0,
                       byte 7, byte 0, byte 0, byte 0, byte 4, byte 0, byte 0, byte 0,
                       byte 1, byte 0, byte 0, byte 0, byte 16, byte 0, byte 0, byte 0};
    (used, request) := styx->Tmsg.unpack(wire);
    if (used != 23 || request == nil || request.tag != 1) {
        raise "fail: valid Styx request rejected";
    }
    for (length = 0; length < len wire; length++) {
        (used, request) = styx->Tmsg.unpack(wire[:length]);
        if (used != 0 || request != nil) {
            raise "fail: truncated Styx request";
        }
    }
    sys->print("PASS Styx impossible-frame guards\n");
}
