#!/usr/bin/env python3
#                     S M O K E . P Y
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

"""Install one release package and exercise it outside the build environment."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from vmpkg import extract_archive, run, virtual_display

COMMAND_TIMEOUT_SECONDS = 60


def install(args):
    if os.geteuid() != 0:
        raise ValueError('Package installation requires root in a fresh runtime guest')
    package = args.package.resolve()
    if package.suffix == '.rpm':
        run(['dnf', '-y', '--disablerepo=*', 'install', package])
        files = subprocess.check_output(['rpm', '-qlp', package]).decode().splitlines()
        candidates = [Path(name).parent.parent for name in files if name.endswith('/bin/mged')]
    else:
        if args.destination is None or args.destination.exists():
            raise ValueError('Tar packages require a new --destination directory')
        destination = args.destination.resolve()
        extract_archive(package, destination)
        candidates = [path.parent.parent for path in destination.rglob('bin/mged')]
    if len(candidates) != 1:
        raise ValueError('Expected exactly one installed bin/mged')
    print('Installed prefix: ' + str(candidates[0]))


def check(args):
    if os.geteuid() == 0:
        raise ValueError('Run smoke checks as the builder account')
    prefix = args.prefix.resolve()
    for name in ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'TCL_LIBRARY', 'TK_LIBRARY', 'QT_PLUGIN_PATH',
                 'QT_QPA_PLATFORM_PLUGIN_PATH', 'BRLCAD_ROOT'):
        os.environ.pop(name, None)
    os.environ['LIBGL_ALWAYS_SOFTWARE'] = '1'
    for path in prefix.rglob('*'):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open('rb') as stream:
            if stream.read(4) != b'\x7fELF':
                continue
        result = subprocess.run(['ldd', str(path)], text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        if 'not found' in result.stdout or (result.returncode and 'not a dynamic executable' not in result.stdout):
            raise ValueError('Unresolved runtime dependencies for {}:\n{}'.format(path, result.stdout))
    with tempfile.TemporaryDirectory(prefix='vmpkg-smoke-') as temporary:
        root = Path(temporary)
        with virtual_display(root / 'Xvfb.log'):
            commands = """
if {[catch {
    package require Tk
    update
    in smoke.s sph 0 0 0 10
    r smoke.r u smoke.s
} message]} {puts stderr $message; exit 1}
q
"""
            script = root / 'smoke.tcl'
            script.write_text(commands)
            # Source Tcl directly to avoid MGED's command-input escaping.
            subprocess.run([prefix / 'bin/mged', '-c', root / 'smoke.g', 'source', script],
                           check=True, timeout=COMMAND_TIMEOUT_SECONDS)
            run([prefix / 'bin/rt', '-s', '32', '-o', root / 'smoke.pix',
                 root / 'smoke.g', 'smoke.r'], timeout=COMMAND_TIMEOUT_SECONDS)
            run([prefix / 'bin/g2asc', root / 'smoke.g', root / 'smoke.asc'],
                timeout=COMMAND_TIMEOUT_SECONDS)
            run([prefix / 'bin/qged', '--help'], timeout=COMMAND_TIMEOUT_SECONDS)
            if not (root / 'smoke.pix').stat().st_size or not (root / 'smoke.asc').stat().st_size:
                raise ValueError('Rendering or database conversion produced empty output')
    print('Runtime library, Tcl/Tk, geometry, rendering, conversion, and Qt executable checks passed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest='action', required=True)
    installer = actions.add_parser('install')
    installer.add_argument('--package', type=Path, required=True)
    installer.add_argument('--destination', type=Path)
    installer.set_defaults(function=install)
    checker = actions.add_parser('check')
    checker.add_argument('--prefix', type=Path, required=True)
    checker.set_defaults(function=check)
    args = parser.parse_args()
    try:
        args.function(args)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
