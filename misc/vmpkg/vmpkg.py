#!/usr/bin/env python3
#                     V M P K G . P Y
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

"""Prepare release-builder media and run it with bhyve or QEMU/KVM."""

import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import lzma
import os
from pathlib import Path
import platform
import re
import select
import shlex
import shutil
import stat
import subprocess
import tarfile
import tempfile
import urllib.request

HERE = Path(__file__).resolve().parent
DEFAULT_CPUS = 4
DEFAULT_MEMORY_MIB = 24576
DEFAULT_DISK_GIB = 200
DEFAULT_SSH_PORT = 2222
FREEBSD_DISK_GIB = 48
FREEBSD_MEMORY_MIB = 8192
GUESTS = ('rocky', 'freebsd')
BLOCK_BYTES = 1024 * 1024
GIB_BYTES = 1024 ** 3
MIN_DISK_GIB = 16
XVFB_START_SECONDS = 60
XVFB_STOP_SECONDS = 10
XVFB_SCREEN = "1280x960x24"


def run(command, dry_run=False, check=True, **kwargs):
    command = [str(arg) for arg in command]
    print('+ ' + ' '.join(shlex.quote(arg) for arg in command), flush=True)
    if not dry_run:
        return subprocess.run(command, check=check, **kwargs)
    return None


def digest(path):
    result = hashlib.sha256()
    with open(path, 'rb') as stream:
        for chunk in iter(lambda: stream.read(BLOCK_BYTES), b''):
            result.update(chunk)
    return result.hexdigest()


def write_checksums(output):
    checksums = output / 'SHA256SUMS'
    pending = output / '.SHA256SUMS.tmp'
    with pending.open('w') as stream:
        for artifact in sorted(output.rglob('*')):
            if artifact not in (checksums, pending) and artifact.is_file():
                stream.write(digest(artifact) + '  ' + artifact.relative_to(output).as_posix() + '\n')
    pending.replace(checksums)


def extract_archive(archive, destination):
    destination = destination.resolve()
    with tarfile.open(archive, 'r:*') as source:
        # Git snapshots may contain symlinks. Require link targets to stay
        # within the archive, and reject special files before extraction.
        for member in source.getmembers():
            path = (destination / member.name).resolve()
            if not path.is_relative_to(destination) or member.isdev() or member.isfifo():
                raise ValueError('Unsafe source archive entry: ' + member.name)
            if member.issym() or member.islnk():
                base = path.parent if member.issym() else destination
                if not (base / member.linkname).resolve().is_relative_to(destination):
                    raise ValueError('Source archive link escapes its root: ' + member.name)
        source.extractall(destination, filter='data')


@contextmanager
def virtual_display(log_path):
    previous = os.environ.get('DISPLAY')
    with log_path.open('w') as log:
        read_fd, write_fd = os.pipe()
        try:
            server = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0',
                                       XVFB_SCREEN, '-nolisten', 'tcp'],
                                      pass_fds=(write_fd,), stdout=log, stderr=log)
        except OSError:
            os.close(read_fd)
            raise
        finally:
            os.close(write_fd)
        try:
            ready, _, _ = select.select([read_fd], [], [], XVFB_START_SECONDS)
            display = os.read(read_fd, 32).decode().strip() if ready else ''
            if not display.isdigit():
                raise ValueError('Xvfb failed to start; see ' + str(log_path))
            os.environ['DISPLAY'] = ':' + display
            yield
        finally:
            server.terminate()
            try:
                server.wait(timeout=XVFB_STOP_SECONDS)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
            finally:
                os.close(read_fd)
            if previous is None:
                os.environ.pop('DISPLAY', None)
            else:
                os.environ['DISPLAY'] = previous


@contextmanager
def guest_lock(work):
    with (work / '.running.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def external_path(path):
    path = path.resolve()
    if path.is_relative_to(HERE.parents[1]):
        raise ValueError('Store VM media, caches, and source archives outside the source repository')
    return path


def read_inputs():
    return json.loads((HERE / 'inputs.json').read_text())


def download(item, cache):
    target = cache / item['url'].rsplit('/', 1)[1]
    if target.exists():
        if digest(target) != item['sha256']:
            raise ValueError('Cached file checksum mismatch: ' + str(target))
        return target
    with tempfile.TemporaryDirectory(prefix='.download-', dir=cache) as temporary:
        partial = Path(temporary) / target.name
        with urllib.request.urlopen(item['url'], timeout=120) as source, partial.open('wb') as output:
            shutil.copyfileobj(source, output, BLOCK_BYTES)
        if digest(partial) != item['sha256']:
            raise ValueError('Checksum mismatch for ' + item['url'])
        partial.replace(target)
    return target


def guest_inputs(guest):
    inputs = read_inputs()
    keys = ('freebsd',) if guest == 'freebsd' else ('rocky', 'cmake', 'ninja', 'epel_key')
    return {key: inputs[key] for key in keys}


def fetch(args):
    cache = external_path(args.cache)
    cache.mkdir(parents=True, exist_ok=True)
    for item in guest_inputs(args.guest).values():
        print(download(item, cache))


def package_roots(path):
    return [line.split('#', 1)[0].strip() for line in path.read_text().splitlines()
            if line.split('#', 1)[0].strip()]


def public_key(key):
    fields = key.strip().split()
    if len(fields) < 2 or not re.fullmatch(r'(ssh-|ecdsa-|sk-)[A-Za-z0-9@.-]+', fields[0]):
        raise ValueError('An OpenSSH public key is required')
    if not re.fullmatch(r'[A-Za-z0-9+/=]+', fields[1]) or '\n' in key.strip():
        raise ValueError('Invalid public key')
    return ' '.join(fields[:2])


def kickstart(key):
    template = (HERE / 'kickstart.cfg.in').read_text()
    return template.replace('@SSH_KEY@', public_key(key)).replace(
        '@BASE_PACKAGES@', '\n'.join(package_roots(HERE / 'base-packages.txt')))


def unattended_menu(text, grub):
    # Keep the distribution's stage2 label and bootloader layout intact.
    text = re.sub(r'(?m)^(\s*(?:linuxefi|linux|append)\s+.*)$',
                  r'\1 inst.ks=hd:LABEL=OEMDRV:/ks.cfg console=tty0 console=ttyS0,115200n8', text)
    if grub:
        text = re.sub(r'(?m)^set timeout=.*$', 'set timeout=1', text)
        text = re.sub(r'(?m)^set default=.*$', 'set default="0"', text)
    else:
        text = re.sub(r'(?m)^timeout\s+.*$', 'timeout 10', text)
        text = re.sub(r'(?m)^default\s+.*$', 'default linux', text)
    # Avoid the media-test entry, which can add hours to a serial installation.
    text = text.replace('rd.live.check', '')
    return text


def rewrite_boot_menu(path, grub):
    text = unattended_menu(path.read_text(), grub)
    mode = stat.S_IMODE(path.stat().st_mode)
    # ISO extraction preserves read-only permissions. Keep that mode in the
    # remastered image while allowing the host copy to be edited.
    path.chmod(mode | stat.S_IWUSR)
    try:
        path.write_text(text)
    finally:
        path.chmod(mode)


def seed_iso(work, label):
    run(['xorriso', '-as', 'mkisofs', '-quiet', '-J', '-r', '-V', label,
         '-o', work / 'seed.iso', work / 'seed'])


def prepare_rocky(work, iso, key, capacity):
    seed = work / 'seed'
    seed.mkdir()
    shutil.copytree(HERE, seed / 'vmpkg', ignore=shutil.ignore_patterns('__pycache__', '.gitignore'))
    (seed / 'ks.cfg').write_text(kickstart(key))
    seed_iso(work, 'OEMDRV')
    maps = []
    for entry, grub in [('isolinux/isolinux.cfg', False), ('EFI/BOOT/grub.cfg', True)]:
        target = work / Path(entry).name
        run(['xorriso', '-osirrox', 'on', '-indev', iso, '-extract', '/' + entry, target])
        rewrite_boot_menu(target, grub)
        maps.extend(['-map', target, '/' + entry])
    run(['xorriso', '-indev', iso, '-outdev', work / 'install.iso',
         '-boot_image', 'any', 'replay'] + maps)
    with (work / 'disk.raw').open('xb') as disk:
        disk.truncate(capacity)


def prepare_freebsd(work, media, key, capacity):
    seed = work / 'seed'
    seed.mkdir()
    (seed / 'meta-data').write_text('local-hostname: brlcad-freebsd\n')
    config = {
        'ssh_pwauth': False,
        'users': [{'name': 'builder', 'groups': ['wheel'],
                   'ssh_authorized_keys': [public_key(key)],
                   'sudo': 'ALL=(ALL) NOPASSWD:ALL'}],
        'packages': package_roots(HERE / 'freebsd-packages.txt'),
    }
    (seed / 'user-data').write_text('#cloud-config\n' + json.dumps(config, indent=2) + '\n')
    seed_iso(work, 'cidata')
    with tempfile.TemporaryDirectory(prefix='.freebsd-', dir=work) as temporary:
        image = Path(temporary) / 'base.qcow2'
        with lzma.open(media, 'rb') as source, image.open('wb') as output:
            shutil.copyfileobj(source, output, BLOCK_BYTES)
        run(['qemu-img', 'convert', '-f', 'qcow2', '-O', 'raw', '-S', '4k',
             image, work / 'disk.raw'])
    with (work / 'disk.raw').open('r+b') as disk:
        if os.fstat(disk.fileno()).st_size > capacity:
            raise ValueError('The requested disk capacity is smaller than the FreeBSD image')
        disk.truncate(capacity)


def prepare(args):
    work = external_path(args.work)
    disk_gib = args.disk_gib if args.disk_gib is not None else (
        FREEBSD_DISK_GIB if args.guest == 'freebsd' else DEFAULT_DISK_GIB)
    if disk_gib < MIN_DISK_GIB:
        raise ValueError('The guest disk must have at least {} GiB virtual capacity'.format(MIN_DISK_GIB))
    if work.exists():
        raise ValueError('Use a new work directory: ' + str(work))
    inputs = guest_inputs(args.guest)
    item = inputs[args.guest]
    media = args.cache.resolve() / item['url'].rsplit('/', 1)[1]
    if not media.is_file() or digest(media) != item['sha256']:
        raise ValueError('Run fetch first; the pinned {} image is missing or invalid'.format(args.guest))
    key = public_key(args.public_key.read_text())
    run(['ssh-keygen', '-l', '-f', args.public_key], stdout=subprocess.DEVNULL)
    work.mkdir(parents=True)
    writer = prepare_freebsd if args.guest == 'freebsd' else prepare_rocky
    writer(work, media, key, disk_gib * GIB_BYTES)
    (work / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
    print('Prepared {} guest in {}'.format(args.guest, work))


def work_guest(work):
    receipt = work / 'inputs.json'
    if not receipt.is_file():
        return 'rocky'
    inputs = json.loads(receipt.read_text())
    matches = [guest for guest in GUESTS if guest in inputs]
    if len(matches) != 1:
        raise ValueError('Invalid guest media receipt: ' + str(receipt))
    return matches[0]


def firmware_copy(source, destination, dry_run):
    if not destination.exists():
        print('Copy firmware variables: {} -> {}'.format(source, destination))
        if not dry_run:
            with tempfile.TemporaryDirectory(prefix='.firmware-', dir=destination.parent) as temporary:
                pending = Path(temporary) / destination.name
                shutil.copyfile(source, pending)
                pending.replace(destination)


def qemu_command(args):
    work = args.work.resolve()
    guest = work_guest(work)
    command = ['qemu-system-x86_64', '-enable-kvm', '-machine', 'q35',
               '-cpu', 'host', '-smp', args.cpus, '-m', args.memory_mib,
               '-drive', 'if=pflash,format=raw,readonly=on,file=' + str(args.firmware.resolve()),
               '-drive', 'if=pflash,format=raw,file=' + str(work / 'uefi-vars.fd'),
               '-drive', 'file=' + str(work / 'disk.raw') + ',format=raw,if=virtio,discard=unmap',
               '-netdev', 'user,id=net0,hostfwd=tcp:127.0.0.1:{}-:22'.format(args.ssh_port),
               '-device', 'virtio-net-pci,netdev=net0']
    if getattr(args, 'console_log', None):
        command += ['-display', 'none', '-monitor', 'none',
                    '-chardev', 'file,id=console,append=on,path=' + str(args.console_log),
                    '-serial', 'chardev:console']
    else:
        command += ['-nographic']
    if getattr(args, 'qmp_socket', None):
        command += ['-qmp', 'unix:' + str(args.qmp_socket) + ',server=on,wait=off']
    if args.install:
        command += ['-drive', 'file=' + str(work / 'install.iso') + ',media=cdrom,readonly=on', '-boot', 'once=d']
    if args.install or guest == 'freebsd':
        command += ['-drive', 'file=' + str(work / 'seed.iso') + ',media=cdrom,readonly=on']
    return command


def bhyve_command(args):
    work = args.work.resolve()
    command = ['bhyve', '-AHPD', '-c', args.cpus, '-m', str(args.memory_mib) + 'M',
               '-s', '0:0,hostbridge', '-s', '1:0,lpc', '-s', '2:0,virtio-net,' + args.tap,
               '-s', '3:0,virtio-blk,' + str(work / 'disk.raw'),
               '-l', 'com1,stdio', '-l', 'bootrom,' + str(args.firmware.resolve()) + ',' + str(work / 'uefi-vars.fd')]
    if args.install:
        command += ['-s', '4:0,ahci-cd,' + str(work / 'install.iso')]
    if args.install or work_guest(work) == 'freebsd':
        command += ['-s', '5:0,ahci-cd,' + str(work / 'seed.iso')]
    return command + [args.name]


def launch(args):
    expected_host = 'Linux' if args.action == 'qemu' else 'FreeBSD'
    if not args.dry_run and platform.system() != expected_host:
        raise ValueError(args.action + ' requires a ' + expected_host + ' host')
    args.work = external_path(args.work)
    guest = work_guest(args.work)
    if args.memory_mib is None:
        args.memory_mib = FREEBSD_MEMORY_MIB if guest == 'freebsd' else DEFAULT_MEMORY_MIB
    if guest == 'freebsd' and args.install:
        raise ValueError('The FreeBSD image is preinstalled; omit --install')
    for option in ('console_log', 'qmp_socket'):
        if getattr(args, option, None):
            setattr(args, option, external_path(getattr(args, option)))
    for value in (args.cpus, args.memory_mib):
        if value <= 0:
            raise ValueError('CPU count and memory must be positive')
    # Commas delimit QEMU and bhyve device options, even without a shell.
    for path in (args.work, args.firmware, args.vars_template,
                 getattr(args, 'console_log', None), getattr(args, 'qmp_socket', None)):
        if path is not None and ',' in str(path.resolve()):
            raise ValueError('VM device paths cannot contain commas')
    if not args.dry_run:
        required = ['disk.raw', 'inputs.json']
        if args.install:
            required.append('install.iso')
        if args.install or guest == 'freebsd':
            required.append('seed.iso')
        for name in required:
            if not (args.work / name).is_file():
                raise ValueError('Missing {} in the guest directory; run prepare first'.format(name))
        console_log = getattr(args, 'console_log', None)
        if console_log:
            protected = [args.work / name for name in required + ['uefi-vars.fd']]
            protected += [args.firmware, args.vars_template]
            if any(console_log == path.resolve() for path in protected):
                raise ValueError('The serial log path overlaps a guest input')
    command = qemu_command(args) if args.action == 'qemu' else bhyve_command(args)
    if args.dry_run:
        run(command, dry_run=True)
        return
    vm_device = None
    if args.action == 'bhyve':
        if not re.fullmatch(r'[A-Za-z0-9_-]+', args.name) or ',' in args.tap:
            raise ValueError('Invalid bhyve guest name or tap interface')
        vm_device = Path('/dev/vmm') / args.name
        if vm_device.exists():
            raise ValueError('A bhyve guest with this name already exists')
    elif not 0 < args.ssh_port < 65536:
        raise ValueError('Invalid SSH forwarding port')
    with guest_lock(args.work):
        if getattr(args, 'qmp_socket', None) and args.qmp_socket.exists():
            raise ValueError('QMP socket path already exists: ' + str(args.qmp_socket))
        firmware_copy(args.vars_template, args.work / 'uefi-vars.fd', False)
        try:
            result = run(command, check=False)
            allowed_status = (0, 1, 2) if args.action == 'bhyve' else (0,)
            if result.returncode not in allowed_status:
                raise subprocess.CalledProcessError(result.returncode, command)
        finally:
            if vm_device is not None and vm_device.exists():
                run(['bhyvectl', '--destroy', '--vm=' + args.name])


def clone(args):
    source = external_path(args.work)
    destination = external_path(args.output)
    if destination.is_relative_to(source):
        raise ValueError('Keep the clone outside its source guest directory')
    if destination.exists():
        raise ValueError('Use a new clone directory')
    with guest_lock(source):
        required = ['disk.raw', 'uefi-vars.fd', 'inputs.json']
        freebsd = work_guest(source) == 'freebsd'
        if freebsd:
            required.append('seed.iso')
        for name in required:
            if not (source / name).is_file():
                raise ValueError('The source guest is missing ' + name)
        destination.mkdir(parents=True)
        run(['qemu-img', 'convert', '-f', 'raw', '-O', 'raw', '-S', '4k',
             source / 'disk.raw', destination / 'disk.raw'])
        if freebsd:
            shutil.copyfile(source / 'seed.iso', destination / 'seed.iso')
        shutil.copyfile(source / 'uefi-vars.fd', destination / 'uefi-vars.fd')
        shutil.copyfile(source / 'inputs.json', destination / 'inputs.json')


def snapshot_one(source, target, origin):
    source = source.resolve()
    run(['git', 'clone', '--no-local', '--depth', '1', source, target], stdout=subprocess.DEVNULL)
    run(['git', '-C', target, 'remote', 'set-url', 'origin', origin])
    result = subprocess.check_output(['git', '-C', source, 'ls-files', '--cached', '--others',
                                      '--exclude-standard', '-z'])
    for raw in result.split(b'\0'):
        if not raw:
            continue
        name = os.fsdecode(raw)
        original, copied = source / name, target / name
        if original.is_dir() and not original.is_symlink():
            # Gitlinks are populated by bext on demand in the Linux guest.
            continue
        if copied.exists() or copied.is_symlink():
            copied.unlink()
        if original.exists() or original.is_symlink():
            copied.parent.mkdir(parents=True, exist_ok=True)
            if original.is_symlink():
                copied.symlink_to(os.readlink(original))
            else:
                shutil.copy2(original, copied)
    return subprocess.check_output(['git', '-C', target, 'rev-parse', 'HEAD']).decode().strip()


def snapshot(args):
    components = [(name, source) for name, source in [('brlcad', args.brlcad), ('bext', args.bext)]
                  if source is not None]
    if not components:
        raise ValueError('Select --brlcad, --bext, or both for a source snapshot')
    args.output = external_path(args.output)
    if args.output.exists():
        raise ValueError('Source archive already exists')
    with tempfile.TemporaryDirectory(prefix='vmpkg-sources-') as temporary:
        root = Path(temporary)
        revisions = {}
        for name, source in components:
            revisions[name] = snapshot_one(source, root / name, 'https://github.com/BRL-CAD/' + name)
        (root / 'revisions.json').write_text(json.dumps(revisions, indent=2) + '\n')
        with tempfile.TemporaryDirectory(prefix='.snapshot-', dir=args.output.parent) as temporary_output:
            pending = Path(temporary_output) / args.output.name
            with tarfile.open(pending, 'w:gz') as archive:
                for name in list(revisions) + ['revisions.json']:
                    archive.add(root / name, arcname=name)
            os.link(pending, args.output)
    print(digest(args.output) + '  ' + str(args.output))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest='action', required=True)
    fetch_parser = actions.add_parser('fetch', help='Retrieve and verify pinned guest inputs')
    fetch_parser.add_argument('--guest', choices=GUESTS, default='rocky')
    fetch_parser.add_argument('--cache', type=Path, required=True)
    fetch_parser.set_defaults(function=fetch)
    prepare_parser = actions.add_parser('prepare', help='Prepare a guest disk and provisioning media')
    prepare_parser.add_argument('--guest', choices=GUESTS, default='rocky')
    prepare_parser.add_argument('--cache', type=Path, required=True)
    prepare_parser.add_argument('--work', type=Path, required=True)
    prepare_parser.add_argument('--public-key', type=Path, required=True)
    prepare_parser.add_argument('--disk-gib', type=int)
    prepare_parser.set_defaults(function=prepare)
    for name in ('qemu', 'bhyve'):
        launch_parser = actions.add_parser(name, help='Run the installer or the installed guest')
        launch_parser.add_argument('--work', type=Path, required=True)
        launch_parser.add_argument('--firmware', type=Path, required=True)
        launch_parser.add_argument('--vars-template', type=Path, required=True)
        launch_parser.add_argument('--cpus', type=int, default=DEFAULT_CPUS)
        launch_parser.add_argument('--memory-mib', type=int)
        launch_parser.add_argument('--install', action='store_true')
        launch_parser.add_argument('--dry-run', action='store_true')
        if name == 'qemu':
            launch_parser.add_argument('--ssh-port', type=int, default=DEFAULT_SSH_PORT)
            launch_parser.add_argument('--console-log', type=Path, help='Run without a terminal and log the guest serial output')
            launch_parser.add_argument('--qmp-socket', type=Path, help='Expose a local QEMU monitor socket for diagnostics')
        else:
            launch_parser.add_argument('--tap', required=True)
            launch_parser.add_argument('--name', default='release-builder')
        launch_parser.set_defaults(function=launch)
    clone_parser = actions.add_parser('clone', help='Sparsely clone a shut-down base or prepared guest')
    clone_parser.add_argument('--work', type=Path, required=True)
    clone_parser.add_argument('--output', type=Path, required=True)
    clone_parser.set_defaults(function=clone)
    snapshot_parser = actions.add_parser('snapshot', help='Export current sources, including local changes')
    snapshot_parser.add_argument('--brlcad', type=Path)
    snapshot_parser.add_argument('--bext', type=Path)
    snapshot_parser.add_argument('--output', type=Path, required=True)
    snapshot_parser.set_defaults(function=snapshot)
    args = parser.parse_args()
    try:
        args.function(args)
    except KeyboardInterrupt:
        parser.exit(130, 'VM operation interrupted\n')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
