#!/usr/bin/env python3
#                     T E S T _ B E X T . P Y
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

"""Exercise dependency artifact integrity and independent build consumers."""

import argparse
from contextlib import chdir, nullcontext
import importlib.util
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import bext
import serial
import vmpkg

spec = importlib.util.spec_from_file_location('guest_build', vmpkg.HERE / 'guest-build.py')
guest_build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guest_build)

DEPENDENCY_PROJECT = r'''
cmake_minimum_required(VERSION 3.26)
project(dependency_fixture C)
add_library(answer SHARED answer.c)
add_executable(helper helper.c)
target_link_libraries(helper PRIVATE answer)
set_target_properties(helper PROPERTIES INSTALL_RPATH "$ORIGIN/../../install/lib")
install(TARGETS answer LIBRARY DESTINATION install/lib)
install(FILES answer.h DESTINATION install/include)
install(TARGETS helper RUNTIME DESTINATION noinstall/bin)
install(FILES AnswerConfig.cmake DESTINATION install/lib/cmake/Answer)
file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/install" "${CMAKE_INSTALL_PREFIX}/noinstall")
file(WRITE "${CMAKE_INSTALL_PREFIX}/bext-install-manifest.json"
  "{\"schema\":1,\"platform\":\"${CMAKE_SYSTEM_NAME}\",\"files\":[],\"rpath\":[],\"binary\":[],\"cmake\":[],\"text\":[]}")
file(WRITE "${CMAKE_INSTALL_PREFIX}/bext-content-stamp" "fixture\n")
add_custom_target(bext_finalize ALL
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
  DEPENDS answer helper)
'''

PACKAGE_CONFIG = r'''
get_filename_component(answer_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_library(Answer::Answer SHARED IMPORTED)
set_target_properties(Answer::Answer PROPERTIES
  IMPORTED_LOCATION "${answer_prefix}/lib/libanswer.so"
  INTERFACE_INCLUDE_DIRECTORIES "${answer_prefix}/include")
'''

CONSUMER_PROJECT = r'''
cmake_minimum_required(VERSION 3.26)
project(consumer_fixture C)
find_package(Answer CONFIG REQUIRED
  PATHS "${BRLCAD_EXT_DIR}/install/lib/cmake/Answer" NO_DEFAULT_PATH)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE Answer::Answer)
add_custom_target(regress-repository COMMAND consumer DEPENDS consumer)
add_custom_target(regress-licenses COMMAND consumer DEPENDS consumer)
add_custom_target(check COMMAND consumer DEPENDS consumer)
'''


def remove_tree(path):
    # Published prefixes are deliberately read-only, including directories.
    if path.exists():
        for directory, directories, files in os.walk(path):
            Path(directory).chmod(0o755)
            for name in files:
                member = Path(directory) / name
                if not member.is_symlink():
                    member.chmod(stat.S_IMODE(member.stat().st_mode) | stat.S_IWUSR)
        shutil.rmtree(path)


def fixture_logged(command, name, logs, cwd=None):
    # Artifact behavior is independent of the Rocky release resource logger.
    with (logs / (name + '.log')).open('a') as output:
        vmpkg.run(command, cwd=cwd, stdout=output, stderr=subprocess.STDOUT)


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja') and shutil.which('cc'),
                     'CMake, Ninja, and a C compiler are required')
class ArtifactTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        logger = patch.object(guest_build, 'logged', fixture_logged)
        logger.start()
        cls.addClassCleanup(logger.stop)
        cls.temporary = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temporary.name)
        cls.store = cls.root / 'store'
        cls.work = cls.root / 'producer'
        cls.source = cls.work / 'sources/bext'
        cls.source.parent.mkdir(parents=True)
        # Reuse an existing commit for fixture metadata without creating commits.
        subprocess.run(['git', 'clone', '--shared', '--no-checkout',
                        vmpkg.HERE.parents[1], cls.source], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        (cls.source / 'CMake').mkdir()
        (cls.source / 'CMake/BextReleaseBuild.cmake').write_text('')
        (cls.source / 'CMakeLists.txt').write_text(DEPENDENCY_PROJECT)
        (cls.source / 'answer.c').write_text('int answer(void) { return 7; }\n')
        (cls.source / 'answer.h').write_text('int answer(void);\n')
        (cls.source / 'helper.c').write_text(
            '#include <stdio.h>\n#include "answer.h"\nint main(void) { printf("%d", answer()); }\n')
        (cls.source / 'AnswerConfig.cmake').write_text(PACKAGE_CONFIG)
        subprocess.run(['git', '-C', cls.source, 'add', '.'], check=True)
        (cls.work / 'logs').mkdir()
        cls.scripts = cls.root / 'scripts'
        cls.scripts.mkdir()
        for name in ('bext.py', 'serial.py'):
            shutil.copyfile(vmpkg.HERE / name, cls.scripts / name)
        (cls.scripts / 'environment-lock.json').write_text('{"fixture": true}\n')
        cls.builder = {'role': 'builder', 'lock_sha256': vmpkg.digest(cls.scripts / 'environment-lock.json')}
        cls.environment = bext.environment_identity(cls.builder)
        with patch.object(bext, 'HERE', cls.scripts):
            cls.artifact, cls.prefix, cls.receipt = bext.build(
                cls.source, cls.work, cls.environment, 'bundled', cls.store, 1, guest_build.logged)

    @classmethod
    def tearDownClass(cls):
        remove_tree(cls.root)
        cls.temporary.cleanup()

    def copy_artifact(self):
        directory = tempfile.TemporaryDirectory(dir=self.root)
        self.addCleanup(directory.cleanup)
        copied = Path(directory.name) / 'artifact'
        shutil.copytree(self.artifact, copied)
        return copied

    def test_archive_and_receipt_corruption_rejected(self):
        for name in (bext.PAYLOAD_FILE, bext.RECEIPT_FILE, bext.SOURCES_FILE):
            with self.subTest(name=name):
                copied = self.copy_artifact()
                with (copied / name).open('ab') as stream:
                    stream.write(b'corruption')
                with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                    bext.restore(copied, self.environment, 'bundled', self.store)

    def test_extra_file_rejected(self):
        copied = self.copy_artifact()
        (copied / 'unrecorded').write_text('unexpected')
        with self.assertRaisesRegex(ValueError, 'file set differs'):
            bext.restore(copied, self.environment, 'bundled', self.store)

    def test_recipe_modification_rejected_with_updated_checksums(self):
        copied = self.copy_artifact()
        receipt = json.loads((copied / bext.RECEIPT_FILE).read_text())
        receipt['recipe']['profile'] = 'everything'
        bext.write_json(copied / bext.RECEIPT_FILE, receipt)
        vmpkg.write_checksums(copied)
        with self.assertRaisesRegex(ValueError, 'recipe checksum'):
            bext.restore(copied, self.environment, 'bundled', self.store)

    def test_failed_restore_does_not_publish_a_prefix(self):
        copied = self.copy_artifact()
        receipt = json.loads((copied / bext.RECEIPT_FILE).read_text())
        receipt['files']['install/include/answer.h']['sha256'] = '0' * 64
        bext.write_json(copied / bext.RECEIPT_FILE, receipt)
        vmpkg.write_checksums(copied)
        remove_tree(self.prefix)
        try:
            with self.assertRaisesRegex(ValueError, 'content differs'):
                bext.restore(copied, self.environment, 'bundled', self.store)
            self.assertFalse(self.prefix.exists())
            self.assertFalse(list(self.store.glob('.restore-*')))
        finally:
            bext.restore(self.artifact, self.environment, 'bundled', self.store)

    def test_relative_artifact_and_store_paths(self):
        with chdir(self.root):
            prefix, receipt = bext.restore(self.artifact.relative_to(self.root),
                                           self.environment, 'bundled', Path('store'))
        self.assertEqual(prefix, self.prefix)
        self.assertEqual(receipt, self.receipt)

    def test_reused_prefix_is_read_only(self):
        header = self.prefix / 'install/include/answer.h'
        header.chmod(stat.S_IMODE(header.stat().st_mode) | stat.S_IWUSR)
        bext.restore(self.artifact, self.environment, 'bundled', self.store)
        self.assertFalse(header.stat().st_mode & stat.S_IWUSR)

    def test_environment_profile_and_prefix_mismatches_rejected(self):
        changed = dict(self.environment, lock_sha256='different')
        for environment, profile, store, reason in [
                (changed, 'bundled', self.store, 'environment'),
                (self.environment, 'everything', self.store, 'profile'),
                (self.environment, 'bundled', self.root / 'other', 'prefix')]:
            with self.subTest(reason=reason), self.assertRaisesRegex(ValueError, reason):
                bext.restore(self.artifact, environment, profile, store)

    def test_jobs_do_not_invalidate_completed_build(self):
        with patch.object(bext, 'HERE', self.scripts):
            artifact, prefix, receipt = bext.build(
                self.source, self.work, self.environment, 'bundled', self.store, 2,
                lambda *args: self.fail('A completed dependency build ran again'))
        self.assertEqual((artifact, prefix, receipt), (self.artifact, self.prefix, self.receipt))

    def test_serial_compat_builds_without_release_helpers(self):
        work = self.root / 'legacy-producer'
        (work / 'logs').mkdir(parents=True)
        support = self.source / 'CMake/BextReleaseBuild.cmake'
        support.unlink()
        project = self.source / 'CMakeLists.txt'
        original = project.read_text()
        project.write_text(original.replace('${CMAKE_INSTALL_PREFIX}/bext-content-stamp',
                                            '${CMAKE_BINARY_DIR}/bext-content-stamp'))
        try:
            with patch.object(bext, 'HERE', self.scripts), \
                    serial.build_tools(self.root / 'legacy-tools'):
                artifact, prefix, receipt = bext.build(
                    self.source, work, self.environment, 'bundled', self.store, 1,
                    guest_build.logged, serial_compat=True)
            self.assertEqual(subprocess.check_output([prefix / 'noinstall/bin/helper']), b'7')
            self.assertIn('serial_driver_sha256', receipt['recipe'])
            self.assertNotIn('-DBEXT_DISCARD_WORK=ON', receipt['recipe']['cmake_options'])
            self.assertTrue((work / 'bext-build/CMakeCache.txt').is_file())
            self.assertEqual((prefix / 'bext-content-stamp').read_bytes(),
                             (work / 'bext-build/bext-content-stamp').read_bytes())
            self.assertEqual(bext.restore(artifact, self.environment, 'bundled', self.store)[1], receipt)
        finally:
            support.write_text('')
            project.write_text(original)

    def test_dirty_source_changes_recipe(self):
        before = bext.source_identity(self.source)
        header = self.source / 'answer.h'
        original = header.read_text()
        try:
            header.write_text(original + '/* changed interface */\n')
            self.assertNotEqual(before, bext.source_identity(self.source))
        finally:
            header.write_text(original)

    def test_cached_prefix_corruption_rejected(self):
        library = self.prefix / 'install/lib/libanswer.so'
        original = library.read_bytes()
        original_mode = stat.S_IMODE(library.stat().st_mode)
        library.chmod(original_mode | stat.S_IWUSR)
        try:
            library.write_bytes(b'corrupted library')
            with self.assertRaisesRegex(ValueError, 'content differs'):
                bext.restore(self.artifact, self.environment, 'bundled', self.store)
        finally:
            library.write_bytes(original)
            library.chmod(original_mode)
            bext.seal(self.prefix)

    def test_populating_a_pinned_submodule_does_not_change_recipe(self):
        revision = subprocess.check_output(['git', '-C', self.source, 'rev-parse', 'HEAD']).decode().strip()
        subprocess.run(['git', '-C', self.source, 'update-index', '--add', '--cacheinfo',
                        '160000,' + revision + ',dependency'], check=True)
        dependency = self.source / 'dependency'
        try:
            before = bext.source_identity(self.source)
            dependency.mkdir()
            (dependency / 'populated-source').write_text('downloaded dependency')
            self.assertEqual(before, bext.source_identity(self.source))
        finally:
            subprocess.run(['git', '-C', self.source, 'update-index', '--force-remove',
                            'dependency'], check=True)
            remove_tree(dependency)

    def test_component_snapshots_are_independent(self):
        def snapshot_component(source, target, origin):
            shutil.copytree(source, target)
            return self.receipt['bext_ref']

        for name in ('bext', 'brlcad'):
            archive = self.root / (name + '-only.tar.gz')
            args = argparse.Namespace(output=archive, bext=None, brlcad=None)
            setattr(args, name, self.source)
            with patch.object(vmpkg, 'snapshot_one', side_effect=snapshot_component) as snapshot:
                vmpkg.snapshot(args)
            snapshot.assert_called_once()
            with tarfile.open(archive) as source:
                roots = {member.name.split('/')[0] for member in source}
            self.assertEqual(roots, {name, 'revisions.json'})

    def test_empty_snapshot_request_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Select'):
            vmpkg.snapshot(argparse.Namespace(output=self.root / 'empty.tar.gz', bext=None, brlcad=None))

    def test_restore_without_producer_and_build_two_consumers(self):
        # Hide the producer trees, then exercise the real restore and CMake path.
        hidden = self.root / 'hidden-producer'
        self.work.rename(hidden)
        remove_tree(self.prefix)
        try:
            artifact = hidden / 'bext-artifact'
            prefix, receipt = bext.restore(artifact, self.environment, 'bundled', self.store)
            self.assertEqual(subprocess.check_output([prefix / 'noinstall/bin/helper']), b'7')
            for increment in (1, 2):
                consumer = self.root / ('consumer-' + str(increment))
                source = consumer / 'sources/brlcad'
                source.mkdir(parents=True)
                (source / 'CMakeLists.txt').write_text(CONSUMER_PROJECT)
                (source / 'main.c').write_text(
                    '#include <stdio.h>\n#include "answer.h"\n'
                    'int main(void) { printf("%d", answer() + ' + str(increment) + '); }\n')
                (consumer / 'logs').mkdir()
                with patch.object(guest_build, 'virtual_display', return_value=nullcontext()):
                    guest_build.build_brlcad(consumer, 1, prefix)
                self.assertEqual(subprocess.check_output([consumer / 'brlcad-build/consumer']),
                                 str(7 + increment).encode())
                self.assertEqual(bext.inventory(prefix), receipt['files'])
            self.assertFalse(prefix.stat().st_mode & stat.S_IWUSR)
        finally:
            hidden.rename(self.work)

    def test_consumer_dispatch_never_builds_dependencies(self):
        source_archive = self.root / 'consumer-sources.tar.gz'
        with tarfile.open(source_archive, 'w:gz') as archive:
            archive.add(self.source, arcname='brlcad')
        args = argparse.Namespace(stage='brlcad', sources=source_archive,
                                  work=self.root / 'dispatch', jobs=1, profile='bundled',
                                  bext_artifact=self.artifact, bext_store=self.store, serial_compat=False)
        with patch.object(guest_build, 'builder_receipt', return_value=self.builder), \
                patch.object(guest_build, 'HERE', self.scripts), \
                patch.object(bext, 'build', side_effect=AssertionError('Dependencies rebuilt')), \
                patch.object(guest_build, 'build_brlcad') as consumer, \
                patch.object(guest_build, 'package_build') as packages:
            guest_build.build(args)
        consumer.assert_called_once_with(args.work, 1, self.prefix, serial_compat=False)
        packages.assert_called_once()
        provenance = json.loads((args.work / 'output/provenance.json').read_text())
        self.assertEqual(provenance['bext_artifact']['recipe_sha256'], self.receipt['recipe_sha256'])


@unittest.skipUnless(os.name == 'posix' and shutil.which('cmake'),
                     'POSIX permissions and CMake are required')
class StagingTest(unittest.TestCase):
    def test_legacy_staging_reseals_after_configure_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / 'sdk'
            header = prefix / 'install/include/answer.h'
            header.parent.mkdir(parents=True)
            header.write_text('original\n')
            bext.seal(prefix)
            original = bext.inventory(prefix)
            try:
                with self.assertRaisesRegex(RuntimeError, 'configure failed'):
                    with bext.writable_staging(prefix):
                        staged = root / 'staged'
                        subprocess.run(['cmake', '-E', 'copy_directory', prefix / 'install', staged],
                                       check=True)
                        (staged / 'include/answer.h').write_text('private copy\n')
                        raise RuntimeError('configure failed')
                self.assertEqual(bext.inventory(prefix), original)
                self.assertFalse(header.stat().st_mode & stat.S_IWUSR)
                self.assertFalse(header.parent.stat().st_mode & stat.S_IWUSR)
            finally:
                remove_tree(prefix)

    def test_read_only_sdk_stages_writable_private_copies(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / 'sdk'
            install = prefix / 'install'
            source = root / 'source'
            source.mkdir()
            for name in ('bin/helper', 'lib/nested/keep.txt', 'lib/nested/exclude.txt'):
                path = install / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('original\n')
                path.chmod(0o755 if name.startswith('bin/') else 0o644)
            bext.seal(prefix)
            before = bext.inventory(prefix)
            module = vmpkg.HERE.parents[0] / 'CMake/BRLCAD_ExternalDeps.cmake'
            (source / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 3.26)
project(staging_fixture NONE)
list(APPEND CMAKE_MODULE_PATH "''' + module.parent.as_posix() + '''")
include(BRLCAD_ExternalDeps)
set(BRLCAD_EXT_INSTALL_DIR "${BRLCAD_EXT_DIR}/install")
set(EXCLUDED_PATTERNS "exclude[.]txt$")
initialize_tp_files()
''')
            build = root / 'build'
            try:
                result = subprocess.run(['cmake', '-S', source, '-B', build,
                                         '-DBRLCAD_EXT_DIR=' + str(prefix)], text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertFalse((build / 'lib/nested/exclude.txt').exists())
                for name in ('bin/helper', 'lib/nested/keep.txt'):
                    path = build / name
                    self.assertTrue(path.stat().st_mode & stat.S_IWUSR)
                    self.assertTrue(path.parent.stat().st_mode & stat.S_IWUSR)
                    self.assertEqual(path.stat().st_mode & 0o111,
                                     (install / name).stat().st_mode & 0o111)
                    path.write_text('consumer edit\n')
                self.assertEqual(bext.inventory(prefix), before)
                self.assertFalse(prefix.stat().st_mode & stat.S_IWUSR)
            finally:
                remove_tree(prefix)


if __name__ == '__main__':
    unittest.main()
