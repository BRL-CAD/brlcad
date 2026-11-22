#!/usr/bin/env python3
#                     G U E S T - E N V . P Y
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

"""Resolve a builder RPM bundle once, then provision from its exact bytes."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

from vmpkg import HERE, digest, download, package_roots, read_inputs, run

RECEIPT = Path('/opt/vmpkg/environment.json')
TOOLS = Path('/opt/vmpkg-tools/cmake')
BUILD_TOOLS = Path('/opt/vmpkg-tools/bin')
PYTHON_INTERPRETER = '/usr/bin/python3.11'
ROCKY_SIGNING_KEY = Path('/etc/pki/rpm-gpg/RPM-GPG-KEY-rockyofficial')
RPM_MULTILIB_EXCLUDE = '*.i686'
BUILDER_ARCHIVES = ('cmake', 'ninja')
NINJA_EXECUTABLE_MODE = 0o755


def inventory():
    return sorted(subprocess.check_output([
        'rpm', '-qa', '--qf', '%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}\n'
    ]).decode().splitlines())


def base_policy():
    return {name: digest(HERE / name) for name in
            ('inputs.json', 'base-packages.txt', 'kickstart.cfg.in')}


def import_signing_keys(epel_key):
    run(['rpmkeys', '--import', ROCKY_SIGNING_KEY, epel_key])


def lock(args):
    archives = {}
    if args.role == 'builder':
        for name in BUILDER_ARCHIVES:
            archive = getattr(args, name)
            if archive is None or digest(archive) != read_inputs()[name]['sha256']:
                raise ValueError('Builder bundles require the pinned --' + name + ' archive')
            archives[name] = archive.resolve()
        if len({archive.name for archive in archives.values()}) != len(archives):
            raise ValueError('Builder archive basenames must be distinct')
    bundle = args.bundle.resolve()
    if bundle.exists():
        raise ValueError('Use a new bundle directory')
    bundle.mkdir(parents=True)
    key = download(read_inputs()['epel_key'], bundle)
    import_signing_keys(key)
    rpms = bundle / 'rpms'
    rpms.mkdir()
    repos = bundle / 'repos'
    repos.mkdir()
    shutil.copyfile(HERE / 'rocky.repo', repos / 'rocky.repo')
    roots = package_roots(HERE / (args.role + '-packages.txt'))
    # Resolve the install transaction to retain conditional dependencies and
    # upgrades required by packages already installed in the base image.
    run(['dnf', '-y', '--setopt=reposdir=' + str(repos), '--disablerepo=*',
         '--enablerepo=vmpkg-*', '--setopt=install_weak_deps=False',
         '--setopt=keepcache=True',
         '--exclude=' + RPM_MULTILIB_EXCLUDE, 'install', '--downloadonly',
         '--destdir=' + str(rpms)] + roots)
    packages = sorted(rpms.glob('*.rpm'))
    if not packages:
        raise ValueError('DNF did not download any packages')
    run(['rpmkeys', '--checksig'] + packages)
    manifest = {
        'schema': 1,
        'role': args.role,
        'base_rpms': inventory(),
        'base_policy': base_policy(),
        'roots': roots,
        'repository_config_sha256': digest(repos / 'rocky.repo'),
        'rpms': {path.name: {'sha256': digest(path), 'nevra': subprocess.check_output([
            'rpm', '-qp', '--qf', '%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}',
            str(path)]).decode()} for path in packages}
    }
    for name, archive in archives.items():
        shutil.copyfile(archive, bundle / archive.name)
        manifest[name] = {'file': archive.name, 'sha256': read_inputs()[name]['sha256']}
    (bundle / 'lock.json').write_text(json.dumps(manifest, sort_keys=True, indent=2) + '\n')
    print('Preserve the entire bundle; lock.json identifies the resolved environment.')


def signing_key(bundle):
    item = read_inputs()['epel_key']
    key = bundle / item['url'].rsplit('/', 1)[1]
    if digest(key) != item['sha256']:
        raise ValueError('EPEL signing key checksum mismatch')
    return key


def validate_bundle(bundle):
    signing_key(bundle)
    manifest = json.loads((bundle / 'lock.json').read_text())
    if manifest.get('schema') != 1 or not manifest.get('rpms'):
        raise ValueError('Unsupported or empty RPM lock')
    if manifest.get('base_policy') != base_policy():
        raise ValueError('Base image policy differs from the RPM lock')
    if manifest.get('role') not in ('builder', 'runtime'):
        raise ValueError('Unsupported package role')
    if manifest['roots'] != package_roots(HERE / (manifest['role'] + '-packages.txt')):
        raise ValueError('Package roots changed; resolve a new bundle')
    if manifest['repository_config_sha256'] != digest(HERE / 'rocky.repo'):
        raise ValueError('Repository policy differs from the RPM lock')
    if set(manifest['rpms']) != {path.name for path in (bundle / 'rpms').glob('*.rpm')}:
        raise ValueError('RPM file set differs from the lock')
    for name, item in manifest['rpms'].items():
        if Path(name).name != name or digest(bundle / 'rpms' / name) != item['sha256']:
            raise ValueError('RPM checksum mismatch: ' + name)
    if manifest['role'] == 'builder':
        for name in BUILDER_ARCHIVES:
            archive = manifest.get(name)
            if (not archive or Path(archive['file']).name != archive['file'] or
                    archive['sha256'] != read_inputs()[name]['sha256'] or
                    digest(bundle / archive['file']) != archive['sha256']):
                raise ValueError(name + ' input differs from the pinned input')
    return manifest


def verify(args=None):
    receipt = json.loads(RECEIPT.read_text())
    if inventory() != receipt['installed_rpms']:
        raise ValueError('Installed RPMs differ from the provisioned environment')
    if receipt['role'] == 'builder':
        for name, executable in [('cmake', TOOLS / 'bin/cmake'), ('ninja', BUILD_TOOLS / 'ninja')]:
            if digest(executable) != receipt.get(name + '_executable_sha256'):
                raise ValueError('Installed ' + name + ' executable changed')
        for name in ('python', 'python3'):
            if os.readlink(BUILD_TOOLS / name) != PYTHON_INTERPRETER:
                raise ValueError('Build Python selection changed')
    return receipt


def install(args):
    bundle = args.bundle.resolve()
    manifest = validate_bundle(bundle)
    lock_hash = digest(bundle / 'lock.json')
    if RECEIPT.exists():
        if verify()['lock_sha256'] != lock_hash:
            raise ValueError('Guest was provisioned using a different lock; use a fresh base guest')
        return
    import_signing_keys(signing_key(bundle))
    if inventory() != manifest['base_rpms']:
        raise ValueError('Base RPM inventory differs from the resolver guest; use the same base image')
    packages = sorted((bundle / 'rpms').glob('*.rpm'))
    run(['rpmkeys', '--checksig'] + packages)
    # DNF cleanup must not remove the locked RPM inputs after the transaction.
    run(['dnf', '-y', '--disablerepo=*', '--setopt=install_weak_deps=False',
         '--setopt=keepcache=True',
         '--setopt=localpkg_gpgcheck=1', 'install'] + packages)
    receipt = {'role': manifest['role'], 'lock_sha256': lock_hash, 'installed_rpms': inventory()}
    if manifest['role'] == 'builder':
        TOOLS.mkdir(parents=True, exist_ok=False)
        run(['tar', '-xzf', bundle / manifest['cmake']['file'], '--strip-components=1', '-C', TOOLS])
        run([TOOLS / 'bin/cmake', '--version'])
        receipt['cmake_executable_sha256'] = digest(TOOLS / 'bin/cmake')
        BUILD_TOOLS.mkdir(exist_ok=False)
        ninja = BUILD_TOOLS / 'ninja'
        with zipfile.ZipFile(bundle / manifest['ninja']['file']) as archive:
            ninja.write_bytes(archive.read('ninja'))
        ninja.chmod(NINJA_EXECUTABLE_MODE)
        run([ninja, '--version'])
        receipt['ninja_executable_sha256'] = digest(ninja)
        for name in ('python', 'python3'):
            (BUILD_TOOLS / name).symlink_to(PYTHON_INTERPRETER)
    RECEIPT.write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n')
    shutil.copyfile(bundle / 'lock.json', HERE / 'environment-lock.json')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest='action')
    lock_parser = actions.add_parser('lock')
    lock_parser.add_argument('--bundle', type=Path, required=True)
    lock_parser.add_argument('--cmake', type=Path)
    lock_parser.add_argument('--ninja', type=Path)
    lock_parser.add_argument('--role', choices=['builder', 'runtime'], default='builder')
    lock_parser.set_defaults(function=lock)
    install_parser = actions.add_parser('install')
    install_parser.add_argument('--bundle', type=Path, required=True)
    install_parser.set_defaults(function=install)
    actions.add_parser('verify').set_defaults(function=verify)
    args = parser.parse_args()
    if not args.action:
        parser.error('Select lock, install, or verify')
    if args.action != 'verify' and os.geteuid() != 0:
        parser.error('lock and install must run as root in the disposable guest')
    try:
        args.function(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
