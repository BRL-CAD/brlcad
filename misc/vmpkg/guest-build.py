#!/usr/bin/env python3
#                     G U E S T - B U I L D . P Y
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

"""Build a source snapshot and retain packages, provenance, and failure logs."""

import argparse
from contextlib import nullcontext
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

import bext
import serial
from vmpkg import HERE, digest, extract_archive, virtual_display, write_checksums


def logged(command, name, logs, cwd=None):
    print('+ ' + ' '.join(str(value) for value in command), flush=True)
    with (logs / (name + '.log')).open('a') as output:
        subprocess.run(['/usr/bin/time', '-v', '-o', str(logs / (name + '.resources'))] +
                       [str(value) for value in command], cwd=cwd, check=True,
                       stdout=output, stderr=subprocess.STDOUT)


def package_build(root, logs):
    brlcad_build = root / 'brlcad-build'
    output = root / 'output'
    checksums = output / 'SHA256SUMS'
    checksums.unlink(missing_ok=True)
    archive_formats = [('TGZ', '.tar.gz'), ('TBZ2', '.tar.bz2')]
    for config, label, destination, generators in [
            ('CPackConfig.cmake', 'package', output, [('RPM', '.rpm')] + archive_formats),
            ('CPackSourceConfig.cmake', 'package-source', output / 'source',
             archive_formats + [('ZIP', '.zip')])]:
        destination.mkdir(exist_ok=True)
        for generator, suffix in generators:
            name = label + '-' + generator.lower()
            staging = root / name
            options = []
            if config == 'CPackConfig.cmake' and generator != 'RPM':
                options = ['-D', 'CPACK_PRE_BUILD_SCRIPTS=' + str(HERE / 'package-relocate.cmake')]
            logged(['cpack', '--config', brlcad_build / config,
                    '-G', generator, '-B', staging] + options, name, logs)
            packages = list(staging.glob('*' + suffix))
            if not packages:
                raise ValueError('CPack produced no ' + name + ' package')
            for package in packages:
                package.replace(destination / package.name)
            shutil.rmtree(staging)
    write_checksums(output)
    print('Packages: ' + str(output))


def builder_receipt():
    spec = importlib.util.spec_from_file_location('guest_env', HERE / 'guest-env.py')
    environment = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(environment)
    receipt = environment.verify()
    if receipt['role'] != 'builder':
        raise ValueError('A builder RPM bundle must be installed before building')
    return receipt


def prepare_work(args, receipt, environment, dependencies):
    root = args.work.resolve()
    store = args.bext_store.resolve()
    if store.is_relative_to(root) or root.is_relative_to(store):
        raise ValueError('Keep --bext-store separate from the disposable work directory')
    root.mkdir(parents=True, exist_ok=True)
    fingerprint = digest(args.sources)
    recipe = {'sources_sha256': fingerprint, 'stage': args.stage, 'profile': args.profile,
              'environment': environment,
              'bext_store': str(store),
              'bext_artifact': dependencies}
    if args.serial_compat:
        recipe['serial_compat'] = True
    identity = root / 'recipe.json'
    if identity.exists():
        if json.loads(identity.read_text()) != recipe:
            raise ValueError('Build inputs or options changed; use a new work directory')
    else:
        if any(root.iterdir()):
            raise ValueError('Use an empty work directory')
        extract_archive(args.sources, root / 'sources')
        identity.write_text(json.dumps(recipe, sort_keys=True, indent=2) + '\n')
    sources = root / 'sources'
    logs = root / 'logs'
    logs.mkdir(exist_ok=True)
    output = root / 'output'
    output.mkdir(exist_ok=True)
    shutil.copyfile(HERE / 'environment-lock.json', output / 'environment-lock.json')
    provenance = {'sources_sha256': fingerprint, 'environment': receipt,
                  'build_jobs': args.jobs, 'profile': args.profile, 'stage': args.stage}
    if args.serial_compat:
        provenance['serial_compat'] = True
    for name in ('brlcad', 'bext'):
        if (sources / name).is_dir():
            provenance[name + '_ref'] = subprocess.check_output(
                ['git', '-C', sources / name, 'rev-parse', 'HEAD']).decode().strip()
    return root, provenance


def build_brlcad(root, jobs, bext_output, serial_compat=False):
    sources, logs = root / 'sources', root / 'logs'
    if not (sources / 'brlcad/CMakeLists.txt').is_file():
        raise ValueError('BRL-CAD build needs a BRL-CAD source snapshot')
    brlcad_build = root / 'brlcad-build'
    os.environ['CTEST_PARALLEL_LEVEL'] = str(jobs)
    test_options = []
    if serial_compat:
        ctest = shutil.which('ctest')
        if not ctest:
            raise ValueError('Serial compatibility requires CTest')
        test_options.append('-DCMAKE_CTEST_COMMAND=' + ctest)
    staging = bext.writable_staging(bext_output) if serial_compat else nullcontext()
    with staging:
        logged(['cmake', '-S', sources / 'brlcad', '-B', brlcad_build, '-G', 'Ninja',
                '-DCMAKE_BUILD_TYPE=Release', '-DBRLCAD_EXT_DIR=' + str(bext_output),
                '-DBRLCAD_ENABLE_QT=ON', '-DBRLCAD_X11_PROVIDER=SYSTEM',
                '-DPython3_EXECUTABLE=/usr/bin/python3.11'] + test_options, 'brlcad-configure', logs)
    logged(['cmake', '--build', brlcad_build, '--parallel', str(jobs)], 'brlcad-build', logs)
    logged(['cmake', '--build', brlcad_build, '--target', 'regress-repository',
            'regress-licenses', '--parallel', str(jobs)], 'repository-checks', logs)
    os.environ['LIBGL_ALWAYS_SOFTWARE'] = '1'
    with virtual_display(logs / 'Xvfb.log'):
        logged(['cmake', '--build', brlcad_build, '--target', 'check',
                '--parallel', str(jobs)], 'tests', logs)


def build(args):
    if os.geteuid() == 0:
        raise ValueError('Run builds as the builder account')
    receipt = builder_receipt()
    environment = bext.environment_identity(receipt)
    dependencies = None
    if args.bext_artifact:
        artifact = args.bext_artifact.resolve()
        bext_output, dependency_receipt = bext.restore(
            artifact, environment, args.profile, args.bext_store)
        dependencies = {'receipt_sha256': digest(artifact / bext.RECEIPT_FILE),
                        'archive_sha256': dependency_receipt['archives'][bext.PAYLOAD_FILE]}
    root, provenance = prepare_work(args, receipt, environment, dependencies)
    if args.bext_artifact is None:
        artifact, bext_output, dependency_receipt = bext.build(
            root / 'sources/bext', root, environment, args.profile,
            args.bext_store.resolve(), args.jobs, logged, serial_compat=args.serial_compat)
        print('Dependency artifact: ' + str(artifact))
    output = root / 'output'
    shutil.copyfile(artifact / bext.RECEIPT_FILE, output / bext.RECEIPT_FILE)
    provenance['bext_ref'] = dependency_receipt['bext_ref']
    provenance['bext_artifact'] = {
        'recipe_sha256': dependency_receipt['recipe_sha256'],
        'receipt_sha256': digest(artifact / bext.RECEIPT_FILE),
        'archive_sha256': dependency_receipt['archives'][bext.PAYLOAD_FILE],
        'bext_ref': dependency_receipt['bext_ref']}
    bext.write_json(output / 'provenance.json', provenance)
    if args.stage != 'bext':
        build_brlcad(root, args.jobs, bext_output, serial_compat=args.serial_compat)
    bext.validate_prefix(bext_output, dependency_receipt['files'])
    if args.stage != 'bext':
        package_build(root, root / 'logs')
    else:
        write_checksums(output)
    print('Logs: ' + str(root / 'logs'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', nargs='?', choices=['all', 'bext', 'brlcad'], default='all')
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--serial-compat', action='store_true',
                        help='Build older bext releases serially, retaining dependency work trees')
    parser.add_argument('--profile', choices=['bundled', 'everything'], default='bundled')
    parser.add_argument('--bext-artifact', type=Path,
                        help='Published bext artifact directory; never rebuild dependencies')
    parser.add_argument('--bext-store', type=Path, default=bext.STORE_DEFAULT,
                        help='Stable dependency prefix parent, shared by producer and consumer')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.serial_compat:
        args.jobs = 1
    if args.stage == 'brlcad' and args.bext_artifact is None:
        parser.error('brlcad requires --bext-artifact')
    if args.stage == 'bext' and args.bext_artifact is not None:
        parser.error('bext produces an artifact; use brlcad or all to consume one')
    try:
        tools = nullcontext()
        if args.serial_compat:
            work = args.work.resolve()
            tools = serial.build_tools(work.with_name(work.name + '-serial-tools'))
        with tools:
            build(args)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\nSee the work directory logs for command output.\n')


if __name__ == '__main__':
    main()
