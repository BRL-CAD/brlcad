#!/usr/bin/env python3
#                     B E X T . P Y
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

"""Build, publish, and verify independently reusable dependency artifacts."""

import hashlib
from contextlib import contextmanager
import json
import os
from pathlib import Path
import platform
import shutil
import stat
import subprocess
import tarfile
import tempfile

from vmpkg import HERE, digest, extract_archive, guest_lock, write_checksums

ARTIFACT_SCHEMA = 1
RECEIPT_FILE = 'bext-receipt.json'
PAYLOAD_FILE = 'bext.tar.gz'
SOURCES_FILE = 'bext-sources.tar.gz'
STORE_DEFAULT = Path.home() / '.local/share/vmpkg/bext'
BUILD_ENVIRONMENT = ('CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'CPATH',
                     'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
                     'LD_LIBRARY_PATH', 'CMAKE_PREFIX_PATH', 'PKG_CONFIG_PATH')


def write_json(path, value):
    path.write_text(json.dumps(value, sort_keys=True, indent=2) + '\n')


def identity(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True,
                                     separators=(',', ':')).encode()).hexdigest()


def inventory(root):
    records = {}
    for directory, directories, files in os.walk(root, followlinks=False):
        for name in sorted(directories + files):
            path = Path(directory) / name
            relative = path.relative_to(root).as_posix()
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode):
                if not path.resolve().is_relative_to(root.resolve()):
                    raise ValueError('Dependency symlink escapes the prefix: ' + relative)
                records[relative] = {'link': os.readlink(path)}
            elif stat.S_ISREG(mode):
                records[relative] = {'sha256': digest(path), 'execute': mode & 0o111}
            elif stat.S_ISDIR(mode):
                records[relative] = {'directory': True}
            else:
                raise ValueError('Unsupported dependency file: ' + relative)
    return records


def source_identity(source):
    # Gitlinks identify dependencies that the snapshot has not populated yet.
    gitlinks = subprocess.check_output(
        ['git', '-C', source, 'ls-files', '--stage', '-z']).decode().split('\0')
    gitlinks = sorted(item for item in gitlinks if item.startswith('160000 '))
    submodules = {item.split('\t', 1)[1] for item in gitlinks}
    names = subprocess.check_output(['git', '-C', source, 'ls-files', '--cached',
                                     '--others', '--exclude-standard', '-z']).decode().split('\0')
    records = {}
    for name in filter(None, names):
        if any(name == module or name.startswith(module + '/') for module in submodules):
            continue
        path = source / name
        if path.is_symlink():
            records[name] = {'link': os.readlink(path)}
        elif path.is_file():
            records[name] = {'sha256': digest(path), 'execute': path.stat().st_mode & 0o111}
        elif not path.exists():
            records[name] = {'missing': True}
    revision = subprocess.check_output(['git', '-C', source, 'rev-parse', 'HEAD']).decode().strip()
    return identity({'revision': revision, 'files': records, 'gitlinks': gitlinks})


def environment_identity(receipt):
    compilers = {}
    for variable, default in [('CC', 'cc'), ('CXX', 'c++')]:
        executable = shutil.which(os.environ.get(variable, default))
        if not executable:
            raise ValueError('Compiler is unavailable: ' + variable)
        compilers[variable] = {
            'sha256': digest(executable),
            'version': subprocess.check_output([executable, '--version']).decode().strip(),
            'target': subprocess.check_output([executable, '-dumpmachine']).decode().strip()}
    return {'lock_sha256': receipt['lock_sha256'],
            'platform': platform.system(), 'architecture': platform.machine(),
            'compilers': compilers,
            'environment': {name: os.environ.get(name, '') for name in BUILD_ENVIRONMENT}}


def cmake_options(profile, serial_compat=False):
    options = ['-DCMAKE_BUILD_TYPE=Release', '-DENABLE_ALL=ON',
               '-DGIT_SHALLOW_CLONE=ON', '-DEXT_BUILD_ALWAYS=OFF',
               '-DPython3_EXECUTABLE=/usr/bin/python3.11']
    if not serial_compat:
        options += ['-DBEXT_DISCARD_WORK=ON', '-DBEXT_LINK_JOBS=1']
    if profile == 'everything':
        options += ['-DUSE_APPLESEED=ON', '-DUSE_OSPRAY=ON']
    return options


def recipe(source, environment, profile, store, serial_compat=False):
    inputs = {'schema': ARTIFACT_SCHEMA, 'source_sha256': source_identity(source),
            'environment': environment, 'profile': profile,
            'cmake_options': cmake_options(profile, serial_compat),
            'driver_sha256': digest(HERE / 'bext.py'),
            'prefix_root': str(store.resolve())}
    if serial_compat:
        inputs['serial_driver_sha256'] = digest(HERE / 'serial.py')
    return inputs


def seal(prefix):
    # BRL-CAD stages private copies; its consumers must not rewrite shared inputs.
    for directory, directories, files in os.walk(prefix, topdown=False):
        for name in directories + files:
            path = Path(directory) / name
            if not path.is_symlink():
                path.chmod(stat.S_IMODE(path.stat().st_mode) & ~0o222)
    prefix.chmod(stat.S_IMODE(prefix.stat().st_mode) & ~0o222)


@contextmanager
def writable_staging(prefix):
    # Older consumers preserve source modes when staging files for rewriting.
    # Serialize this temporary permission change and always reseal the SDK.
    install = prefix / 'install'
    with guest_lock(prefix.parent):
        try:
            for directory, _, files in os.walk(install):
                for path in [Path(directory)] + [Path(directory) / name for name in files]:
                    if not path.is_symlink():
                        path.chmod(stat.S_IMODE(path.stat().st_mode) | stat.S_IWUSR)
            yield
        finally:
            seal(install)


def validate_prefix(prefix, records):
    if inventory(prefix) != records:
        raise ValueError('Dependency prefix content differs from its receipt')
    for name in ('install', 'noinstall'):
        if not (prefix / name).is_dir():
            raise ValueError('Dependency artifact is missing ' + name)
    manifest = json.loads((prefix / 'bext-install-manifest.json').read_text())
    if (not isinstance(manifest, dict) or manifest.get('schema') != 1 or
            manifest.get('platform') != platform.system()):
        raise ValueError('Invalid dependency install manifest')
    install = prefix / 'install'
    for category in ('files', 'rpath', 'binary', 'cmake', 'text'):
        paths = manifest.get(category)
        if not isinstance(paths, list):
            raise ValueError('Dependency install manifest is missing ' + category)
        for name in paths:
            if not isinstance(name, str):
                raise ValueError('Invalid dependency install manifest path')
            path = install / name
            if (Path(name).is_absolute() or not path.resolve().is_relative_to(install.resolve()) or
                    not path.exists()):
                raise ValueError('Invalid dependency install manifest path: ' + name)
    if not (prefix / 'bext-content-stamp').is_file():
        raise ValueError('Dependency artifact has no finalization content stamp')


def artifact_receipt(artifact, environment, profile, store):
    expected = {}
    for line in (artifact / 'SHA256SUMS').read_text().splitlines():
        checksum, name = line.split('  ', 1)
        path = artifact / name
        if (name in expected or path.is_symlink() or not path.is_file() or
                not path.resolve().is_relative_to(artifact.resolve())):
            raise ValueError('Invalid dependency checksum path: ' + name)
        if digest(path) != checksum:
            raise ValueError('Dependency artifact checksum mismatch: ' + name)
        expected[name] = checksum
    actual = {path.relative_to(artifact).as_posix() for path in artifact.rglob('*')
              if path.is_file() and path != artifact / 'SHA256SUMS'}
    if set(expected) != actual:
        raise ValueError('Dependency artifact file set differs from its checksum manifest')
    receipt = json.loads((artifact / RECEIPT_FILE).read_text())
    if not isinstance(receipt, dict) or receipt.get('schema') != ARTIFACT_SCHEMA:
        raise ValueError('Unsupported dependency artifact schema')
    if not all(name in receipt for name in ('recipe', 'recipe_sha256', 'files', 'archives', 'bext_ref')):
        raise ValueError('Incomplete dependency artifact receipt')
    if not isinstance(receipt['files'], dict) or not isinstance(receipt['archives'], dict):
        raise ValueError('Invalid dependency artifact inventory')
    inputs = receipt['recipe']
    required_inputs = ('environment', 'profile', 'prefix_root')
    if not isinstance(inputs, dict) or not all(name in inputs for name in required_inputs):
        raise ValueError('Incomplete dependency build recipe')
    if identity(inputs) != receipt['recipe_sha256']:
        raise ValueError('Dependency recipe checksum mismatch')
    if inputs['environment'] != environment:
        raise ValueError('Dependency artifact builder environment does not match')
    if inputs['profile'] not in ('bundled', 'everything') or (
            profile == 'everything' and inputs['profile'] != 'everything'):
        raise ValueError('Dependency artifact does not provide the requested profile')
    if inputs['prefix_root'] != str(store.resolve()):
        raise ValueError('Dependency artifact requires its recorded --bext-store prefix')
    for name in (PAYLOAD_FILE, SOURCES_FILE, 'environment-lock.json'):
        if digest(artifact / name) != receipt['archives'][name]:
            raise ValueError('Dependency artifact checksum mismatch: ' + name)
    if receipt['archives']['environment-lock.json'] != environment['lock_sha256']:
        raise ValueError('Dependency artifact environment lock checksum mismatch')
    return receipt


def restore(artifact, environment, profile, store):
    artifact, store = artifact.resolve(), store.resolve()
    receipt = artifact_receipt(artifact, environment, profile, store)
    store.mkdir(parents=True, exist_ok=True)
    prefix = store / receipt['recipe_sha256']
    with guest_lock(store):
        if prefix.is_symlink():
            raise ValueError('Dependency prefix must not be a symlink')
        if prefix.exists():
            validate_prefix(prefix, receipt['files'])
        else:
            with tempfile.TemporaryDirectory(prefix='.restore-', dir=store) as temporary:
                staged = Path(temporary) / 'prefix'
                extract_archive(artifact / PAYLOAD_FILE, staged)
                validate_prefix(staged, receipt['files'])
                # Extracting at a temporary path is safe: nothing runs until rename.
                staged.rename(prefix)
        seal(prefix)
    return prefix, receipt


def build(source, root, environment, profile, store, jobs, logged, serial_compat=False):
    store = store.resolve()
    inputs = recipe(source, environment, profile, store, serial_compat)
    build_id = identity(inputs)
    artifact = root / 'bext-artifact'
    if artifact.exists():
        receipt = artifact_receipt(artifact, environment, profile, store)
        if receipt['recipe'] != inputs:
            raise ValueError('Dependency build inputs changed; use a new work directory')
        prefix, receipt = restore(artifact, environment, profile, store)
        return artifact, prefix, receipt
    store.mkdir(parents=True, exist_ok=True)
    prefix = store / build_id
    with guest_lock(store):
        # An incomplete build can resume only from its original work directory.
        owner = root / 'bext-recipe.json'
        if owner.exists():
            if json.loads(owner.read_text()) != inputs:
                raise ValueError('Dependency build inputs changed; use a new work directory')
        elif prefix.exists():
            raise ValueError('Dependency prefix already exists; select its published artifact')
        else:
            write_json(owner, inputs)
        if not serial_compat and not (source / 'CMake/BextReleaseBuild.cmake').is_file():
            raise ValueError('Source snapshot needs bext release-build support; '
                             'use --serial-compat for older releases')
        # Capture sources before configuration populates submodules or generates files.
        source_archive = root / SOURCES_FILE
        if not source_archive.exists():
            with tarfile.open(source_archive.with_suffix('.tmp'), 'w:gz') as archive:
                archive.add(source, arcname='bext')
            source_archive.with_suffix('.tmp').replace(source_archive)
        build_dir = root / 'bext-build'
        parallel_options = [] if serial_compat else ['-DBEXT_BUILD_JOBS=' + str(jobs)]
        logged(['cmake', '-S', source, '-B', build_dir, '-G', 'Ninja',
                '-DCMAKE_INSTALL_PREFIX=' + str(prefix)] + parallel_options + inputs['cmake_options'],
               'bext-configure', root / 'logs')
        logged(['cmake', '--build', build_dir, '--parallel', '1'],
               'bext-build', root / 'logs')
        content_stamp = prefix / 'bext-content-stamp'
        if serial_compat and not content_stamp.exists():
            shutil.copyfile(build_dir / content_stamp.name, content_stamp)
        records = inventory(prefix)
        validate_prefix(prefix, records)
        with tempfile.TemporaryDirectory(prefix='.publish-', dir=root) as temporary:
            staged = Path(temporary) / 'artifact'
            staged.mkdir()
            with tarfile.open(staged / PAYLOAD_FILE, 'w:gz') as archive:
                for path in sorted(prefix.iterdir()):
                    archive.add(path, arcname=path.name)
            shutil.copyfile(source_archive, staged / SOURCES_FILE)
            shutil.copyfile(HERE / 'environment-lock.json', staged / 'environment-lock.json')
            shutil.copytree(root / 'logs', staged / 'logs')
            receipt = {'schema': ARTIFACT_SCHEMA, 'recipe': inputs,
                       'recipe_sha256': build_id, 'files': records,
                       'bext_ref': subprocess.check_output(
                           ['git', '-C', source, 'rev-parse', 'HEAD']).decode().strip(),
                       'build_jobs': jobs,
                       'archives': {name: digest(staged / name) for name in
                                    (PAYLOAD_FILE, SOURCES_FILE, 'environment-lock.json')}}
            write_json(staged / RECEIPT_FILE, receipt)
            write_checksums(staged)
            staged.rename(artifact)
        seal(prefix)
    return artifact, prefix, receipt
