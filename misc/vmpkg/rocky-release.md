# Rocky Linux release builders

These recipes prepare an x86-64 Rocky Linux 8.10 guest for release packaging
under FreeBSD/bhyve or Linux/QEMU/KVM. See the [VM guide](README.md) for
shared host setup and source validation.

All downloaded media, RPM bundles, disks, source archives, and results belong
**outside the source repository**. The host utility rejects repository-local
media/output locations. No images or private keys are stored here.

## Inputs and environment policy

`inputs.json` pins the Rocky DVD, Kitware CMake 3.31.8 and Ninja 1.11.1 binary
archives, and EPEL 8 public signing key by SHA-256 and records their upstream
sources. Retrieval verifies the pinned
bytes, including cached downloads. An upstream replacement fails verification;
review and update the pins deliberately. HTTPS and the checked-in hashes are
the trust anchors for these downloads. There is no automatic checksum refresh.

The guest is small even though installation uses the complete DVD: the DVD
lets Anaconda resolve an explicit base package list offline without groups or
weak dependencies. Installation records the resulting RPM inventory. The
installer may add required boot/platform packages; its pinned ISO and resulting
inventory are part of the environment definition.

`base-packages.txt`, `builder-packages.txt`, and `runtime-packages.txt` list
package roots. `rocky.repo` restricts initial builder/runtime package resolution
to Rocky 8.10 BaseOS, AppStream, and PowerTools, plus a restricted EPEL 8 source
for the XCB cursor development packages and the X11 test driver
`xdotool` with its `libxdo` dependency.
The Rocky key shipped by the pinned base image and the checksummed EPEL key
are imported before recording the base inventory. Resolution selects only
x86-64 and architecture-independent RPMs. These online repositories can change.
**A package list alone is not a version lock.** The separate `lock`
operation saves every resolved RPM, its NEVRA, its SHA-256, the base inventory,
and repository policy in `lock.json`. It downloads DNF's proposed install
transaction, including upgrades and conditional dependencies needed on the
base image. Keep this entire bundle as a release
input. Installing it disables all online repositories and verifies signatures,
checksums, package membership, and the original base inventory. Subsequent
builds verify the installed inventory and CMake/Ninja executables against a receipt.

A first resolution must run on a disposable base guest, because FreeBSD has no
native DNF solver. No guessed transitive version lock is checked in. Save the
first successful lock and its RPM bytes in external artifact storage; future
releases can deliberately reuse or replace them. Recreate the base guest if
base inputs change. Changing a package policy requires a new lock.

The builder selects GCC Toolset 13, Python 3.11, and the pinned CMake and Ninja.
Rocky 8's Ninja 1.8.2 cannot handle LLVM's generated dependency-log rules.
The runtime role includes graphics libraries and smoke-test tools, with no
compiler or development headers. Both use the same base image. This records
and replays build inputs; byte-identical package reproduction is not promised.

## Host preparation

Both hosts need Python 3.11.8 or newer, Git, OpenSSH tools, and xorriso. Cloning
raw disks with the provided utility also needs `qemu-img`; native ZFS snapshots
and clones are an alternative on FreeBSD. Use firmware with Secure Boot off.
Firmware code and variable-template files must be a matching pair.

On FreeBSD, install `bhyve-firmware` and enable the `vmm` kernel module when
ready to run guests. Prepare a tap attached to a bridge or NAT network with
DHCP and external connectivity. Choosing the host interface is a host-specific
administrative step; these scripts do not alter host networking. Pass the tap
name explicitly. See the [FreeBSD virtualization handbook](https://docs.freebsd.org/en/books/handbook/virtualization/).

On Linux, install QEMU system emulation, `qemu-img`, OVMF firmware, and enable
access to `/dev/kvm`. KVM is mandatory; there is no software-emulation fallback.
QEMU uses user-mode networking and forwards SSH to loopback only. No bridge is
required. Typical firmware locations are `/usr/share/OVMF` or
`/usr/share/edk2/ovmf`; FreeBSD firmware is usually under
`/usr/local/share/uefi-firmware`. Supply actual paths rather than copying
firmware from another host.

Default guest limits are four vCPUs, 24 GiB RAM, and a 200 GiB **sparse virtual**
disk. These are initial allowances, not measured minimums. The installed base
uses only a fraction of that disk. LLVM links may determine the RAM floor.
The original DVD and remastered installer each consume additional host space.
Reclaim remastered installation media after saving a working base if needed.

## Retrieve and prepare installation media

Run from the BRL-CAD source root. Substitute an external writable directory:

```sh
vmpkg_root=/path/to/release-work
mkdir -p "$vmpkg_root/cache"
ssh-keygen -t ed25519 -f "$vmpkg_root/builder-key"
python3 misc/vmpkg/vmpkg.py fetch --cache "$vmpkg_root/cache"
python3 misc/vmpkg/vmpkg.py prepare \
    --cache "$vmpkg_root/cache" --work "$vmpkg_root/base" \
    --public-key "$vmpkg_root/builder-key.pub"
```

`prepare` creates a fresh sparse raw disk, an OEMDRV seed ISO containing the
Kickstart and guest scripts, and a remastered installation DVD. It preserves
Rocky's boot layout and stage2 label while adding unattended serial boot
arguments. Public-key login is enabled for `builder`; password login has no
configured password. That account has passwordless sudo inside this dedicated
guest. The private key stays on the host. The Kickstart partitions only `vda`,
the disk supplied by the runners. Do not attach other writable disks during
installation.

## Install and boot with either hypervisor

For Linux, substitute matching OVMF paths:

```sh
python3 misc/vmpkg/vmpkg.py qemu --work "$vmpkg_root/base" --install \
    --firmware /path/to/OVMF_CODE.fd --vars-template /path/to/OVMF_VARS.fd
```

For FreeBSD, substitute the prepared tap and matching firmware:

```sh
sudo python3 misc/vmpkg/vmpkg.py bhyve --work "$vmpkg_root/base" --install \
    --tap tap0 --firmware /path/to/BHYVE_UEFI.fd \
    --vars-template /path/to/BHYVE_UEFI_VARS.fd
```

Installation powers off when complete. Rerun the same command **without
`--install`** to boot the installed system. Each guest gets a writable UEFI
variables copy. Installation media are detached on subsequent boots. Add
`--dry-run` to either launch command to inspect its command without starting a
VM. Override `--cpus`, `--memory-mib`, and QEMU's `--ssh-port` as needed.

QEMU SSH uses `builder@127.0.0.1`, port 2222 by default. With bhyve, use the DHCP
address assigned on the prepared guest network and port 22. Keep SSH host-key
verification enabled. The guest has no display server running by default.

Shut down the installed base and preserve it before provisioning packages.
Clone only stopped guests; the utility locks out other runners using the same
work directory and copies raw disks sparsely:

```sh
python3 misc/vmpkg/vmpkg.py clone --work "$vmpkg_root/base" \
    --output "$vmpkg_root/resolver"
```

The clone has its own disk and UEFI variables. Boot it using either runner
without `--install`. Native ZFS clones can replace this copying step. Do not
run external hypervisor commands against a disk while cloning it.

## Resolve and install package locks

For the following SSH/SCP examples, select the running resolver's endpoint:

```sh
vmpkg_host=127.0.0.1
vmpkg_port=2222
vmpkg_key="$vmpkg_root/builder-key"
scp -i "$vmpkg_key" -P "$vmpkg_port" \
    "$vmpkg_root/cache/cmake-3.31.8-linux-x86_64.tar.gz" \
    "$vmpkg_root/cache/ninja-linux.zip" "builder@$vmpkg_host:"
ssh -i "$vmpkg_key" -p "$vmpkg_port" "builder@$vmpkg_host"
```

Inside that guest, resolve both bundles **before installing either**:

```sh
sudo python3.11 /opt/vmpkg/guest-env.py lock --role builder \
    --bundle /var/tmp/builder-bundle \
    --cmake "$PWD/cmake-3.31.8-linux-x86_64.tar.gz" --ninja "$PWD/ninja-linux.zip"
sudo python3.11 /opt/vmpkg/guest-env.py lock --role runtime \
    --bundle /var/tmp/runtime-bundle
sudo tar -C /var/tmp -czf /var/tmp/builder-bundle.tar.gz builder-bundle
sudo tar -C /var/tmp -czf /var/tmp/runtime-bundle.tar.gz runtime-bundle
```

Copy both archives back to external host storage with SCP. Their embedded lock
checksums protect RPM and tool contents; preserve an independent archive checksum
with the release records as well. The resolver can now be stopped and retained
for diagnostics.

Make another clone of the original base, boot it, transfer the builder bundle,
and extract it under `/var/tmp`. Then install it:

```sh
sudo python3.11 /opt/vmpkg/guest-env.py install --bundle /var/tmp/builder-bundle
```

Save this stopped, provisioned builder as a template. Each build can use a new
clone. The saved RPM bundle allows rebuilding that template without resolving
online packages again. Keep the original unprovisioned base for runtime tests.

## Supply current sources and build

Source snapshots include current working changes and Git metadata, allowing
bext to acquire pinned submodules on demand. Ignored build directories and
populated submodules are excluded. Untracked, nonignored files are included;
review source trees before exporting. Choose release commits in each checkout
before snapshotting when producing an actual release. The explicit bext
snapshot is recorded as an override to BRL-CAD's usual pinned revision.

On the host:

```sh
python3 misc/vmpkg/vmpkg.py snapshot --brlcad . --bext ../bext \
    --output "$vmpkg_root/sources.tar.gz"
scp -i "$vmpkg_key" -P "$vmpkg_port" "$vmpkg_root/sources.tar.gz" \
    "builder@$vmpkg_host:"
```

Inside a fresh provisioned builder clone:

```sh
sh /opt/vmpkg/build.sh --sources "$PWD/sources.tar.gz" \
    --work "$PWD/release-build" --jobs 4 --profile everything
```

`everything` requests the Appleseed and OSPRay stacks as well as standard bext
groups, including LLVM. `bundled` selects standard groups without those two
experimental stacks and does not build LLVM. BRL-CAD's enabled features follow
its configure options; building an optional dependency does not enable every
BRL-CAD experimental feature.

The script activates GCC Toolset, sets a conservative x86-64 compiler baseline,
uses shallow dependency clones, configures independent bext build/output
directories, and builds one dependency
at a time with bounded inner parallelism. Completed working trees are discarded;
Tcl/Tk/Itk trees are deferred until dependency finalization. Sources, completed
outputs, and logs remain. Changing sources, options, or environment requires a
new work directory; rerunning with the same inputs resumes a failed build.
The job limit can change without invalidating the build recipe.

For older bext releases without `CMake/BextReleaseBuild.cmake`, add
`--serial-compat`:

```sh
sh /opt/vmpkg/build.sh --sources "$PWD/sources.tar.gz" \
    --work "$PWD/release-build" --serial-compat --profile bundled
```

This explicit fallback forces one job for dependency builds, BRL-CAD, and tests,
including Make, Ninja, and CTest commands with hardcoded parallel flags. It uses
external build-tool wrappers, including Make selected by GCC's link-time
optimizer, and leaves release sources unchanged. It omits bext's newer
concurrency and cleanup options, retaining dependency working trees, so allow
more disk space. During BRL-CAD configuration it temporarily permits owner writes
to the dependency install tree so older consumers stage writable private copies,
then reseals the tree even if configuration fails. The store lock serializes
this step; dependency contents are verified again before packaging.
It also copies a legacy finalization stamp from the build directory into the
artifact prefix when bext does not publish that stamp there itself.
The fallback still requires bext's install manifest and content
stamp support. Its driver identity is recorded in the dependency recipe; use a
new work directory when switching modes. The default remains the bext-aware
workflow, including when using `--jobs 1`. Artifact consumers may also use
`--serial-compat` to enforce serial BRL-CAD builds.

It then builds BRL-CAD with Qt and system X11, runs repository/license targets
and the `check` target under Xvfb with software GL, and packages binary RPM,
TGZ, and TBZ2 files plus source TGZ, TBZ2, and ZIP archives separately. Binary
tar packages resolve staged SDK paths relative to their CMake and pkg-config
files so the development metadata follows the extracted installation. These
use the same CPack configurations as the `package` and `package_source`
targets. The `check` target selects BRL-CAD's production validation
tests; unfiltered CTest also includes intentionally failing and resource-heavy
tests that are not release gates. The job limit also sets
`CTEST_PARALLEL_LEVEL` before configuration and validation so test processes
respect the guest's memory budget.
Successful package staging trees are removed between formats. Binary packages
are saved directly in `output`; source packages are saved in `output/source`.
The recursive `output/SHA256SUMS` covers both sets of packages and their
provenance files, including source-archive identity, revision IDs, and the
environment lock.
Command output and resource records are in `release-build/logs`; resource files
come from GNU time and do not measure the sum of all concurrent process memory.
Use guest/host memory and disk monitoring during the first full run as well.

Copy all of `release-build/output` and `release-build/logs` to external host
storage and verify `sha256sum -c SHA256SUMS` from the copied output directory
before discarding a build guest. These packages are the persistent release
artifacts; VM disks and intermediate build trees can then be discarded.
The full build also publishes `release-build/bext-artifact`, independently of
the BRL-CAD packages. Retain that entire directory to reuse the dependencies.

## Build and reuse dependencies separately

Export separate source snapshots on the host:

```sh
python3 misc/vmpkg/vmpkg.py snapshot --bext ../bext \
    --output "$vmpkg_root/bext-sources.tar.gz"
python3 misc/vmpkg/vmpkg.py snapshot --brlcad . \
    --output "$vmpkg_root/brlcad-sources.tar.gz"
```

Transfer the bext snapshot to a provisioned builder, then build dependencies:

```sh
sh /opt/vmpkg/build.sh bext --sources "$PWD/bext-sources.tar.gz" \
    --work "$PWD/dependency-build" --jobs 1 --profile bundled
```

Copy the entire `dependency-build/bext-artifact` directory to persistent host
storage and verify `sha256sum -c SHA256SUMS` there. It contains:

* `bext.tar.gz`: the finalized `install` and `noinstall` trees, installation
  manifest, and content stamp. Build tools in `noinstall` are required even
  though they are excluded from BRL-CAD release packages.
* `bext-sources.tar.gz`: the dependency source snapshot captured before
  configuration populates submodules or generates files.
* `bext-receipt.json`: source and recipe identities, builder environment,
  compiler identities and flags, profile, archive checksums, and the complete
  prefix inventory, including file contents, executable bits, and symlinks.
* `environment-lock.json`, build logs/resource records, and `SHA256SUMS`.

In a fresh clone of the same provisioned builder, transfer that artifact
directory and a BRL-CAD snapshot. No bext checkout or build tree is required:

```sh
sh /opt/vmpkg/build.sh brlcad --sources "$PWD/brlcad-sources.tar.gz" \
    --bext-artifact "$PWD/bext-artifact" \
    --work "$PWD/brlcad-build" --jobs 1 --profile bundled
```

This command verifies and restores the artifact, then builds, checks, and
packages BRL-CAD without configuring or building bext. Mismatched environments,
profiles, prefixes, checksums, or existing prefix contents fail explicitly;
there is no dependency-build or network-fetch fallback. An `everything`
artifact can serve a `bundled` consumer, while the reverse is rejected.

The default dependency store is `$HOME/.local/share/vmpkg/bext`, with a prefix
per recipe digest. Both producer and consumer can set `--bext-store`, but they
must use the same absolute store path: this first version preserves the
installation prefix recorded in RPATHs and configuration files. Arbitrary
relocation is not supported. Restored prefixes are read-only, and BRL-CAD
stages private copies. Prefix contents are verified again before packaging.
Different BRL-CAD work directories can consume the same dependency artifact.

The bext recipe excludes BRL-CAD sources and execution job counts. Changes to
bext sources or dependency pins, its build driver/configuration, the builder
lock, compiler identities or relevant environment flags select a new recipe.
Reusing an artifact is explicit rather than an automatic cache search. The
default `all` stage preserves the combined-source workflow above; it can also
consume `--bext-artifact` instead of building dependencies.

Each BRL-CAD output includes the dependency receipt and records the exact
dependency archive and receipt checksums. Keep the corresponding artifact
alongside release provenance so the dependency sources and environment remain
available. Existing work directories from the earlier combined recipe format
require a new work directory. Existing VM templates need the updated scripts
installed before using these commands; new seeds include them automatically.

## Test the installed packages

For each format, clone the original base, install the runtime RPM bundle,
and transfer the package and its checksum file. Verify its checksum first.
In that runtime guest:

```sh
sudo python3.11 /opt/vmpkg/guest-env.py install --bundle /var/tmp/runtime-bundle
sudo /usr/bin/python3.11 /opt/vmpkg/smoke.py install --package /var/tmp/release.rpm
```

For a tar package, use `--package /var/tmp/release.tar.gz` (or `.tar.bz2`) and
`--destination /opt/release-test`, which must not already exist. The installer
prints the resulting prefix. Run checks as the unprivileged builder account:

```sh
/usr/bin/python3.11 /opt/vmpkg/smoke.py check --prefix /path/printed/by/install
```

Checks cover ELF runtime dependency resolution, Tk under Xvfb, database creation,
rendering, conversion, and Qt executable loading via `qged --help`. They do not
yet exercise Qt window interaction or real GPU drivers. The runtime guest must
have no bext outputs or builder environment. Broader distribution and CPU
compatibility testing remains part of release qualification.

Guest deletion returns all storage used by that clone. In-guest deletion can
recycle filesystem space without immediately releasing host blocks; test
TRIM/discard through the chosen storage stack. Do not retain build-time ZFS
snapshots if their old blocks defeat the desired reclamation.

## Offline checks

```sh
python3 -m unittest discover -s misc/vmpkg -p 'test_*.py' -v
python3 ../bext/CMake/tests/test_release_build.py -v
sh -n misc/vmpkg/build.sh
cmake --build .build --target regress-repository regress-licenses
```

The focused tests cover archive boundaries, input checksum failures, Kickstart
rendering, VM command construction, RPM-lock integrity, successful work cleanup,
failed-install retention, deferred cleanup, and build resumption. Artifact tests
compile a small dependency and two independent consumers, exercise restoration
without the producer tree, and reject corrupted or incompatible inputs. They do not
substitute for the first actual installer and full-stack build trials.
