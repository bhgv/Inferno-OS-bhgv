#!/usr/bin/env python3
"""Run both owner emitters' preflight matrix with Python assertions disabled.

This checkpoint deliberately overrides interactive probe filters: a successful
run must cover both priority architectures and every preflight family. It does
not replace the full differential owner-jit.py matrix or prove mapped ownership.
"""
import os
from pathlib import Path
import subprocess
import sys



def main():
    """Run the existing owner fixture under -O and propagate any failed invariant.

    No arguments or ownership are accepted. The child inherits compiler and
    dependency paths, blocks until completion, and raises on compilation,
    emulation or predicate failure. Only the differential sweep is skipped.
    """
    environment = dict(os.environ)
    environment['OWNER_PROBE_ONLY'] = '1'
    environment['OWNER_ARCHES'] = 'amd64 arm64'
    environment.pop('OWNER_PROBE_FAMILY', None)
    fixture = Path(__file__).with_name('owner-jit.py')
    subprocess.run([sys.executable, '-O', str(fixture)],
                   env=environment, check=True)
    print('PASS optimized owner preflight: both architectures, all probe families')


if __name__ == '__main__':
    main()
