# Proton Integration

The override is a standalone host library. Do not copy it into Proton, replace a
Proton library, or maintain a custom Proton build. A launch wrapper adds the
library only to the process tree that needs the optimization.

## Preconditions

- The configured root is already served by a case-insensitive VFS.
- Wrong-case lookup has been tested through that VFS.
- Both x86_64 and i686 libraries are installed for a mixed-architecture Proton
  process tree.
- The installation path has no spaces; this avoids `LD_PRELOAD` tokenization
  ambiguity.

Do not point `WINE_CASEFOLD_ROOTS` at an ordinary case-sensitive directory.

## Install Release Binaries

Download the two `.so` files and `SHA256SUMS` from the
[latest release](https://github.com/ObeseCatLord/wine-casefold-override/releases/latest):

```bash
mkdir -p "$HOME/.local/lib/wine-casefold-override/lib" \
  "$HOME/.local/lib/wine-casefold-override/lib32"
sha256sum -c SHA256SUMS
install -m 0755 libwinecasefold-linux-x86_64.so \
  "$HOME/.local/lib/wine-casefold-override/lib/libwinecasefold.so"
install -m 0755 libwinecasefold-linux-i686.so \
  "$HOME/.local/lib/wine-casefold-override/lib32/libwinecasefold.so"
```

Alternatively, clone the repository and run `./scripts/install.sh`.

## Wrapper

Add these exports immediately before the existing Proton or game launch command:

```bash
CASEFOLD_INSTALL="$HOME/.local/lib/wine-casefold-override"
CASEFOLD_ROOT="/absolute/path/to/case-insensitive/mount/Data"

export WINE_CASEFOLD_ROOTS="$CASEFOLD_ROOT"
export WINE_CASEFOLD_TRACE=1
export LD_PRELOAD="$CASEFOLD_INSTALL/\$LIB/libwinecasefold.so${LD_PRELOAD:+:$LD_PRELOAD}"

# Make the host library visible inside Steam's pressure-vessel container.
export STEAM_COMPAT_MOUNTS="$CASEFOLD_INSTALL${STEAM_COMPAT_MOUNTS:+:$STEAM_COMPAT_MOUNTS}"
export PRESSURE_VESSEL_FILESYSTEMS_RW="$CASEFOLD_INSTALL${PRESSURE_VESSEL_FILESYSTEMS_RW:+:$PRESSURE_VESSEL_FILESYSTEMS_RW}"

exec /path/to/existing-launcher "$@"
```

The backslash before `$LIB` is required. It keeps the token literal until the
dynamic loader selects `lib` for x86_64 or `lib32` for i686.

When bypassing Steam and invoking Proton directly, the pressure-vessel variables
may be unnecessary. Keeping them is harmless when the launcher does not use that
container.

## Validate

1. Start with `WINE_CASEFOLD_TRACE=1`.
2. Confirm stderr contains `configured 1 scoped root(s)` and
   `first scoped FS_IOC_GETFLAGS bypass`.
3. Confirm the game can resolve deliberately wrong-case file names through the VFS.
4. Complete a launch, game load, clean shutdown, and second warm launch.
5. Check the final `bypassed ... casefold ioctls` count and compare launch timings
   from the same start and end markers.
6. Set `WINE_CASEFOLD_TRACE=0` after validation if the summary is no longer needed.

The loader can print benign preload warnings for host processes that cannot see or
use one architecture. The Wine/Proton child processes must still show the trace
markers above.

## Roll Back

Remove `WINE_CASEFOLD_ROOTS`, `WINE_CASEFOLD_TRACE`, and the added `LD_PRELOAD`
entry from the wrapper. No Proton files or prefix files need restoration.

## Build A Local Variant

The implementation is one C source file and has no dependency on Proton internals:

```bash
git clone https://github.com/ObeseCatLord/wine-casefold-override.git
cd wine-casefold-override
make test
make test32
./scripts/install.sh
```

Re-run both test architectures after changing the ioctl boundary. Variadic ioctl
forwarding is ABI-sensitive; unrelated pointer, scalar, no-argument, and faulting
calls must continue to reach the kernel unchanged.
