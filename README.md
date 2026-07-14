# Wine Casefold Override

`wine-casefold-override` is an opt-in Linux `LD_PRELOAD` library for Wine and
Proton processes running against a trusted case-insensitive virtual filesystem,
such as Fluorine's FUSE VFS.

This is **not a modified Proton build**. It works with stock Proton/Wine and does
not replace or patch any runtime files. Source and release binaries are available
at <https://github.com/ObeseCatLord/wine-casefold-override>.

Wine probes directory case behavior with `FS_IOC_GETFLAGS`. On a userspace FUSE
mount, repeating that ioctl millions of times is expensive even when the
filesystem correctly returns `FS_CASEFOLD_FL`. This library answers the probe
in-process only when the opened directory resolves beneath a configured root.

It does not make a filesystem case-insensitive. The configured VFS must already
provide correct case-insensitive lookup and conflict semantics.

The interposer preserves the raw third-argument ABI bits for unrelated pointer,
scalar, legacy, and two-argument ioctls. Scoped result writes use
`process_vm_writev` against the current process so an invalid or read-only output
buffer returns `EFAULT` instead of faulting the process.

## Install

Download the x86_64 and i686 libraries plus `SHA256SUMS` from the
[latest release](https://github.com/ObeseCatLord/wine-casefold-override/releases/latest),
or build both architectures locally:

```bash
./scripts/install.sh
```

The default installation layout is:

```text
~/.local/lib/wine-casefold-override/
├── lib/libwinecasefold.so
└── lib32/libwinecasefold.so
```

The i686 library is needed when the Proton process tree contains 32-bit helpers.
See [Proton integration](docs/PROTON_INTEGRATION.md) for a complete wrapper and
Steam pressure-vessel configuration.

## Build And Test

```bash
make
make test
make build32
make test32
```

The 32-bit targets require the distribution's multilib C development packages.
`make` creates `build/libwinecasefold.so`; `make build32` creates
`build/libwinecasefold32.so`.

## Minimal Use

```bash
export WINE_CASEFOLD_ROOTS="/absolute/path/to/game/Data"
export WINE_CASEFOLD_TRACE=1
export LD_PRELOAD="$HOME/.local/lib/wine-casefold-override/\$LIB/libwinecasefold.so${LD_PRELOAD:+:$LD_PRELOAD}"
```

Keep `\$LIB` literal in the exported path. The dynamic loader expands it to
`lib` or `lib32` for each process architecture.

`WINE_CASEFOLD_ROOTS` is a colon-separated list of absolute paths. A descriptor
must resolve to a configured root or a child of that root. Boundary matching means
`/game/Data2` does not match `/game/Data`.

Do not configure an ordinary case-sensitive ext4/Btrfs directory. A false
casefold result makes Wine skip its fallback directory scan after a wrong-case
direct lookup misses.

With tracing enabled, the library logs its accepted roots, first bypass, and total
bypass count. It never logs individual requested paths.

The scoped response is exactly `FS_CASEFOLD_FL`, matching Fluorine's current
`FS_IOC_GETFLAGS` response. Do not use this with a VFS that needs to report
additional filesystem flags.

## Measured Result

On a 2,000-plus-mod Skyrim VR list using Fluorine and Proton 9, measured from
Proton launch to game focus:

| Configuration | Time |
| --- | ---: |
| Same setup immediately before the interposer | about 5m29s |
| Interposer run 1 | 5m08.1s |
| Interposer warm run 2 | 5m08.2s |

The isolated improvement was about 20.8 seconds, or **6.3% less startup time**.
The number of VFS casefold ioctls observed by focus fell from about 3.13 million
to 192,000, a **93.9% reduction**. Native plugin initialization, assets,
animations, shaders, and XR startup remained the dominant costs, so elapsed time
did not fall in proportion to the ioctl count.

Earlier changes to the launcher, Fluorine, shader compiler path, and XR stack had
already reduced this particular list from about 20m50s on official Fluorine and
45m29s on Windows MO2/USVFS. Those cumulative figures must not be attributed to
this interposer alone.

## License

MIT. See [LICENSE](LICENSE).
