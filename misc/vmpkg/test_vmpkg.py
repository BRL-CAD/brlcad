#!/usr/bin/env python3
#                     T E S T _ V M P K G . P Y
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

"""Offline checks for release inputs, VM commands, and package integrity."""

import argparse
import importlib.util
import io
import json
import lzma
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import vmpkg

spec = importlib.util.spec_from_file_location('guest_env', vmpkg.HERE / 'guest-env.py')
guest_env = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guest_env)


@unittest.skipUnless(os.name == 'posix' and shutil.which('git'),
                     'POSIX symlinks and Git are required')
class SnapshotTest(unittest.TestCase):
    def test_modified_directory_symlink_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source'
            source.mkdir()
            subprocess.run(['git', '-C', source, 'init', '--quiet'], check=True)
            (source / 'first').mkdir()
            (source / 'second').mkdir()
            link = source / 'selected'
            link.symlink_to('first', target_is_directory=True)
            subprocess.run(['git', '-C', source, 'add', '.'], check=True)
            subprocess.run(['git', '-C', source, '-c', 'user.name=Test',
                            '-c', 'user.email=test@example.invalid', 'commit', '--quiet',
                            '-m', 'fixture'], check=True)
            link.unlink()
            link.symlink_to('second', target_is_directory=True)

            target = root / 'snapshot'
            vmpkg.snapshot_one(source, target, 'https://example.invalid/source')

            self.assertTrue((target / 'selected').is_symlink())
            self.assertEqual(os.readlink(target / 'selected'), 'second')


class InputsTest(unittest.TestCase):
    def test_kickstart_is_explicit(self):
        rendered = vmpkg.kickstart('ssh-ed25519 YWJj comment')
        self.assertNotIn('@BASE_PACKAGES@', rendered)
        self.assertIn('%packages --nocore --excludedocs --excludeWeakdeps', rendered)
        self.assertIn('sshkey --username=builder "ssh-ed25519 YWJj"', rendered)
        self.assertNotIn('@core', rendered)

    def test_key_cannot_inject_kickstart(self):
        with self.assertRaises(ValueError):
            vmpkg.kickstart('ssh-ed25519 YWJj\n%post')

    def test_boot_menu_preserves_stage2(self):
        rendered = vmpkg.unattended_menu('set default="1"\nset timeout=60\n'
                                         'linuxefi /vmlinuz inst.stage2=hd:LABEL=Rocky quiet\n', True)
        self.assertIn('set default="0"', rendered)
        self.assertIn('inst.stage2=hd:LABEL=Rocky', rendered)
        self.assertIn('inst.ks=hd:LABEL=OEMDRV:/ks.cfg', rendered)

    def test_read_only_boot_menu_is_rewritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'grub.cfg'
            path.write_text('set default="1"\nset timeout=60\n'
                            'linuxefi /vmlinuz inst.stage2=hd:LABEL=Rocky quiet\n')
            read_only_mode = stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH
            path.chmod(read_only_mode)
            vmpkg.rewrite_boot_menu(path, True)
            self.assertIn('inst.ks=hd:LABEL=OEMDRV:/ks.cfg', path.read_text())
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), read_only_mode)

    def test_checked_download_rejects_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            (cache / 'input').write_bytes(b'corrupt')
            with self.assertRaises(ValueError):
                vmpkg.download({'url': 'https://example.invalid/input', 'sha256': '0' * 64}, cache)

    def test_vm_commands_use_same_raw_disk_and_seed(self):
        args = argparse.Namespace(work=Path('/tmp/guest'), firmware=Path('/tmp/code.fd'),
                                  cpus=4, memory_mib=24576, install=True,
                                  ssh_port=2222, tap='tap0', name='release-builder')
        qemu = [str(value) for value in vmpkg.qemu_command(args)]
        bhyve = [str(value) for value in vmpkg.bhyve_command(args)]
        self.assertIn('-enable-kvm', qemu)
        self.assertIn('3:0,virtio-blk,/tmp/guest/disk.raw', bhyve)
        self.assertTrue(any('seed.iso' in value for value in qemu))
        self.assertTrue(any('seed.iso' in value for value in bhyve))
        args.install = False
        self.assertFalse(any('seed.iso' in str(value) for value in vmpkg.qemu_command(args)))
        self.assertFalse(any('install.iso' in str(value) for value in vmpkg.bhyve_command(args)))

    def test_freebsd_prepare_uses_pinned_image_and_key_only_seed(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            media = work / 'base.qcow2.xz'
            media.write_bytes(lzma.compress(b'fixture'))

            def convert(command):
                self.assertEqual(command[:2], ['qemu-img', 'convert'])
                self.assertEqual(command[-2].read_bytes(), b'fixture')
                command[-1].write_bytes(b'raw')

            with patch.object(vmpkg, 'seed_iso') as seed, patch.object(vmpkg, 'run', side_effect=convert):
                vmpkg.prepare_freebsd(work, media, 'ssh-ed25519 YWJj comment', 64)
            seed.assert_called_once_with(work, 'cidata')
            self.assertEqual((work / 'disk.raw').stat().st_size, 64)
            user_data = (work / 'seed' / 'user-data').read_text()
            self.assertTrue(user_data.startswith('#cloud-config\n'))
            config = json.loads(user_data.split('\n', 1)[1])
            self.assertFalse(config['ssh_pwauth'])
            self.assertEqual(config['users'][0]['ssh_authorized_keys'], ['ssh-ed25519 YWJj'])
            self.assertIn('dejavu', config['packages'])
            self.assertIn('xorg-vfbserver', config['packages'])
            self.assertIn('qt6-svg', config['packages'])
            self.assertEqual(list(vmpkg.guest_inputs('freebsd')), ['freebsd'])

    def test_preinstalled_guest_boot_attaches_only_the_seed(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / 'inputs.json').write_text(json.dumps(vmpkg.guest_inputs('freebsd')))
            args = argparse.Namespace(work=work, firmware=work / 'code.fd', cpus=4,
                                      memory_mib=8192, install=False, ssh_port=2222,
                                      tap='tap0', name='release-builder')
            for command in (vmpkg.qemu_command(args), vmpkg.bhyve_command(args)):
                self.assertTrue(any('seed.iso' in str(value) for value in command))
                self.assertFalse(any('install.iso' in str(value) for value in command))
            args.console_log = work / 'serial.log'
            args.qmp_socket = work / 'monitor.sock'
            command = vmpkg.qemu_command(args)
            self.assertIn('file,id=console,append=on,path=' + str(args.console_log), command)
            self.assertIn('unix:' + str(args.qmp_socket) + ',server=on,wait=off', command)

    def test_archive_escape_rejected_before_extraction(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / 'sources.tar.gz'
            with tarfile.open(archive, 'w:gz') as output:
                member = tarfile.TarInfo('../escape')
                member.size = 1
                output.addfile(member, io.BytesIO(b'x'))
            with self.assertRaises(ValueError):
                vmpkg.extract_archive(archive, root / 'sources')
            self.assertFalse((root / 'escape').exists())

    def test_both_tar_compressions_extract(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for compression in ('gz', 'bz2'):
                archive = root / ('input.tar.' + compression)
                with tarfile.open(archive, 'w:' + compression) as output:
                    member = tarfile.TarInfo('bin/example')
                    member.size = 1
                    output.addfile(member, io.BytesIO(b'x'))
                destination = root / compression
                vmpkg.extract_archive(archive, destination)
                self.assertEqual((destination / 'bin/example').read_bytes(), b'x')

    def test_archive_symlink_escape_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / 'sources.tar.gz'
            with tarfile.open(archive, 'w:gz') as output:
                member = tarfile.TarInfo('link')
                member.type = tarfile.SYMTYPE
                member.linkname = '../../escape'
                output.addfile(member)
            with self.assertRaises(ValueError):
                vmpkg.extract_archive(archive, root / 'sources')


class RpmLockTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.bundle = Path(self.temporary.name)
        key = self.bundle / 'test-key'
        key.write_bytes(b'pinned public key')
        inputs = vmpkg.read_inputs()
        self.inputs = inputs
        inputs['epel_key'] = {'url': 'https://example.invalid/test-key', 'sha256': vmpkg.digest(key)}
        key_policy = patch.object(guest_env, 'read_inputs', return_value=inputs)
        key_policy.start()
        self.addCleanup(key_policy.stop)
        (self.bundle / 'rpms').mkdir()
        rpm = self.bundle / 'rpms/example.rpm'
        rpm.write_bytes(b'pinned RPM bytes')
        self.manifest = {'schema': 1, 'role': 'runtime', 'base_policy': guest_env.base_policy(),
                         'roots': vmpkg.package_roots(vmpkg.HERE / 'runtime-packages.txt'),
                         'repository_config_sha256': vmpkg.digest(vmpkg.HERE / 'rocky.repo'),
                         'rpms': {'example.rpm': {'sha256': vmpkg.digest(rpm), 'nevra': 'example'}}}
        (self.bundle / 'lock.json').write_text(json.dumps(self.manifest))

    def test_valid_bundle(self):
        self.assertEqual(guest_env.validate_bundle(self.bundle), self.manifest)

    def make_builder_bundle(self):
        self.manifest['role'] = 'builder'
        self.manifest['roots'] = vmpkg.package_roots(vmpkg.HERE / 'builder-packages.txt')
        for name in guest_env.BUILDER_ARCHIVES:
            archive = self.bundle / (name + '.archive')
            archive.write_bytes(('pinned ' + name).encode())
            checksum = vmpkg.digest(archive)
            self.inputs[name]['sha256'] = checksum
            self.manifest[name] = {'file': archive.name, 'sha256': checksum}
        (self.bundle / 'lock.json').write_text(json.dumps(self.manifest))

    def test_valid_builder_bundle(self):
        self.make_builder_bundle()
        self.assertEqual(guest_env.validate_bundle(self.bundle), self.manifest)

    def test_modified_ninja_archive_rejected(self):
        self.make_builder_bundle()
        (self.bundle / self.manifest['ninja']['file']).write_bytes(b'modified')
        with self.assertRaisesRegex(ValueError, 'ninja input'):
            guest_env.validate_bundle(self.bundle)

    def test_missing_ninja_pin_rejected(self):
        self.make_builder_bundle()
        del self.manifest['ninja']
        (self.bundle / 'lock.json').write_text(json.dumps(self.manifest))
        with self.assertRaisesRegex(ValueError, 'ninja input'):
            guest_env.validate_bundle(self.bundle)

    def test_builder_archives_cannot_overwrite_each_other(self):
        archives = {}
        for name in guest_env.BUILDER_ARCHIVES:
            path = self.bundle / name / 'archive'
            path.parent.mkdir()
            path.write_bytes(name.encode())
            self.inputs[name]['sha256'] = vmpkg.digest(path)
            archives[name] = path
        args = argparse.Namespace(role='builder', bundle=self.bundle / 'new-bundle', **archives)
        with self.assertRaisesRegex(ValueError, 'basenames must be distinct'):
            guest_env.lock(args)
        self.assertFalse(args.bundle.exists())

    def test_modified_rpm_rejected(self):
        (self.bundle / 'rpms/example.rpm').write_bytes(b'modified')
        with self.assertRaises(ValueError):
            guest_env.validate_bundle(self.bundle)

    def test_modified_key_rejected(self):
        (self.bundle / 'test-key').write_bytes(b'modified key')
        with self.assertRaises(ValueError):
            guest_env.validate_bundle(self.bundle)

    def test_extra_rpm_rejected(self):
        (self.bundle / 'rpms/extra.rpm').write_bytes(b'extra')
        with self.assertRaises(ValueError):
            guest_env.validate_bundle(self.bundle)

    def test_policy_drift_rejected(self):
        with patch.object(guest_env, 'package_roots', return_value=['different']):
            with self.assertRaises(ValueError):
                guest_env.validate_bundle(self.bundle)


if __name__ == '__main__':
    unittest.main()
