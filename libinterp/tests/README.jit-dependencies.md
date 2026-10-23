# Host dependencies for JIT regression tests

Use the same Python interpreter for installing packages and running tests:

```
python3 -m venv /path/to/jit-test-venv
/path/to/jit-test-venv/bin/python -m pip install -r libinterp/tests/requirements-jit.txt
/path/to/jit-test-venv/bin/python libinterp/tests/386-byte-div.py
/path/to/jit-test-venv/bin/python libinterp/tests/arm-vfp.py
```

The pinned package distributions were verified with the current tests.
Capstone disassembles generated instructions, Unicorn executes the supported
instruction subsets, and Keystone assembles ARM instruction fixtures.
The ARM FPSCR/VFP tests require all three packages; installing only Capstone
and Unicorn is insufficient.

Other dependency-based tests are `386-types.py`, `arm-types.py`, `arm-fpscr.py`,
`jit-frame-addresses.py`, `jit-mframe.py`, and `riscv-jit.py`. A host C compiler
is also required. Each test explains its injected boundaries and target/OS
limitations. Passing instruction emulation tests does not establish that a
complete Inferno target boots on hardware.

The use of a separate environment avoids modifying the managed host Python.
A recreated execution environment needs the packages installed again; the
requirements file records how to reproduce the tested dependency set.
