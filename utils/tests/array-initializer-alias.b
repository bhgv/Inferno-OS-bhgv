implement ArrayInitializerAlias;
include "sys.m";

# Standalone real-runtime oracle. No geometry/renderer dependency or heap dump.
ArrayInitializerAlias: module {
    init: fn();
};


# Copied value whose complete contents must survive destination publication.
Point: adt {
    x: int; # First scalar.
    y: int; # Second scalar.
};


# Mutable field used to expose aliases that are not a direct local-name assignment.
Cell: adt {
    points: array of Point; # Old array must survive evaluation of the new initializer.
};


# Report failure without relying on disabled assertions or printing heap contents.
# The caller/runtime harness must require the final PASS line and reject FAIL.
init()
{
    sys := load Sys Sys->PATH;
    cell := ref Cell(array[] of {Point(100, 101), Point(200, 201)});
    alias := cell;
    cell.points = array[] of {alias.points[1], cell.points[0], Point(400, 401)};
    if(cell.points[0].x != 200 || cell.points[0].y != 201 ||
       cell.points[1].x != 100 || cell.points[1].y != 101 ||
       cell.points[2].x != 400 || cell.points[2].y != 401) {
        sys->print("FAIL array initializer ADT field alias\n");
        return;
    }
    values := array[] of {7, 8};
    values = array[] of {values[1], values[0]};
    if(values[0] != 8 || values[1] != 7) {
        sys->print("FAIL array initializer scalar alias\n");
        return;
    }
    strings := array[] of {"old", "kept"};
    strings = array[] of {strings[1], strings[0]};
    if(strings[0] != "kept" || strings[1] != "old") {
        sys->print("FAIL array initializer reference alias\n");
        return;
    }
    filled := array[] of {3, 9};
    filled = array[4] of {* => filled[1]};
    if(len filled != 4 || filled[0] != 9 || filled[3] != 9) {
        sys->print("FAIL array initializer wildcard alias\n");
        return;
    }
    sys->print("PASS array initializer aliases: ADT field, second alias, scalar, references, wildcard\n");
}
