# BRL-CAD build and test VMs

Use a Rocky Linux 8.10 or FreeBSD 15.1 amd64 guest to check a local source
snapshot with the guest's compiler and libraries. The host utility prepares
and launches both guests. The guest check builds with strict warnings, X11,
OpenGL, and Qt, then runs CTest under Xvfb with software rendering. The Rocky
workflow also supports locked RPM provisioning and release packaging.

## Host tools and storage

Run the host utility on Linux or FreeBSD with Python 3.11.8 or later, Git,
OpenSSH, xorriso, and qemu-img. Linux uses QEMU/KVM with access to `/dev/kvm`
and a matching pair of OVMF code and variables files. FreeBSD uses `bhyve`
with a prepared tap interface and matching UEFI firmware. Keep the cache,
guest disks, source snapshots, SSH key, and build outputs in a writable
directory outside the source tree. A 48 GiB sparse disk and 8 GiB of RAM are
starting sizes for FreeBSD builds with two jobs. Rocky's full release build
has larger defaults.
See the [bhyve host setup](rocky-release.md#host-preparation).

## Prepare and boot

Run from the repository root. Set the scratch directory and choose
`freebsd` or `rocky`.

```sh
vm_root=/path/to/vm-work
guest=freebsd
mkdir -p "$vm_root"
ssh-keygen -t ed25519 -f "$vm_root/builder-key"
python3 misc/vmpkg/vmpkg.py fetch --guest "$guest" --cache "$vm_root/cache"
python3 misc/vmpkg/vmpkg.py prepare --guest "$guest" --cache "$vm_root/cache" \
    --work "$vm_root/$guest" --public-key "$vm_root/builder-key.pub"
```

The manifest pins each download by SHA-256. FreeBSD preparation converts the
official UFS QCOW2 image to a sparse raw disk for either hypervisor and writes
a `cidata` seed. On first boot, FreeBSD grows the filesystem and `nuageinit`
provisions the `builder` login from the public key and installs
[the package roots](freebsd-packages.txt). Rocky uses its DVD and a Kickstart
seed. The Rocky installer powers off on completion.

Use the same QEMU command for either guest. Add `--install` **on Rocky's
first boot only**. Substitute the actual firmware paths available on the host.

```sh
python3 misc/vmpkg/vmpkg.py qemu --work "$vm_root/$guest" \
    --firmware /path/to/OVMF_CODE.fd --vars-template /path/to/OVMF_VARS.fd \
    --cpus 4 --memory-mib 8192 --ssh-port 2222
```

Use `--dry-run` to inspect a launch. Use `--console-log "$vm_root/$guest/console.log"`
to run without a terminal and keep serial output. QEMU forwards SSH to
`builder@127.0.0.1` on the requested port. Keep host-key verification enabled.
After login on FreeBSD, check `pkg info` and `which cmake ninja Xvfb`
to confirm first-boot provisioning. Package updates can restart the guest
before SSH becomes available. If `bhyve` exits for a guest reboot, run the
same launch command again. FreeBSD may send first-boot messages to
its video console after the serial kernel messages. A QEMU `--qmp-socket`
allows a `screendump` of that console during a headless launch. Put it in a
private directory so only the VM operator can connect to it.
Use a new work directory after a failed prepare or to repeat the installer.
Shut down a guest before using `clone` to preserve a configured baseline.

## Build and check a source snapshot

On Rocky, [install a builder RPM bundle](rocky-release.md#resolve-and-install-package-locks)
before building. The Rocky release command activates GCC Toolset 13, uses
the pinned tools, starts Xvfb, and runs the build, checks, and packaging.
For the source check alone, run `/opt/vmpkg/build.sh check` with the same
`--sources`, `--work`, and `--jobs` arguments shown below.

To check FreeBSD, snapshot BRL-CAD and optionally a local bext checkout.
When only BRL-CAD is supplied, the project's CMake build fetches its pinned
bext revision. For repeat runs against that same revision, reuse the native
`build/bext_output` directory with `--bext-dir`.

```sh
python3 misc/vmpkg/vmpkg.py snapshot --brlcad . --output "$vm_root/sources.tar.gz"
scp -i "$vm_root/builder-key" -P 2222 "$vm_root/sources.tar.gz" \
    builder@127.0.0.1:
scp -i "$vm_root/builder-key" -P 2222 misc/vmpkg/guest-check.py misc/vmpkg/vmpkg.py \
    builder@127.0.0.1:
ssh -i "$vm_root/builder-key" -p 2222 builder@127.0.0.1
```

In the FreeBSD guest, run:

```sh
python3 guest-check.py --sources sources.tar.gz --work main-check --jobs 2
```

The work directory keeps the source snapshot identity, generated build and
CTest state, and separate configure, build, Xvfb, and test logs. Rerun with
the same sources and CMake options to resume. Use a new work directory for
another branch. `--test-regex` selects a focused CTest run.
Set `CTEST_PARALLEL_LEVEL` to limit test concurrency independently of the build jobs.
`--cmake-arg=-DBRLCAD_ENABLE_STEP=OFF` is useful for a quick rendering-only
trial. For a full check, keep STEP enabled. FreeBSD resolves its packages
at first boot. Preserve the configured guest or record its package inventory
when comparing results across machines. Rocky's RPM lock records the exact
package bytes for replay.

## Check the tooling without a VM

```sh
python3 -B -m unittest discover -s misc/vmpkg -p 'test_*.py'
```

These tests also run with CTest as `vmpkg_tools` when the build finds a
supported Python interpreter. For Rocky package locks, dependency artifacts,
and installed-package smoke tests, use the [Rocky release guide](rocky-release.md).
