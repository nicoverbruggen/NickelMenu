<h1 align="center">NickelMenu</h1>

The easiest way to launch custom scripts, change hidden settings, and run actions on Kobo eReaders.

See the [website](https://pgaskin.net/NickelMenu) and [thread on MobileRead](https://mobileread.com/forums/showthread.php?t=329525) for screenshots and more details.

## Installation
Download packages from [this fork's releases](https://github.com/nicoverbruggen/NickelMenu/releases), or find builds of each commit in [GitHub Actions](https://github.com/nicoverbruggen/NickelMenu/actions).

For firmware 4.x, copy `KoboRoot.tgz` into the `.kobo` folder of your eReader. For firmware 5.x or 6.x, use `Kobo.tgz`. Safely eject the device and let it install. Do not rename the packages: their archive paths differ.

After it installs, you will find a new menu item named `NickelMenu` with further instructions which you can also read [here](./res/doc).

To uninstall NickelMenu, just create a new file named `uninstall` in `.adds/nm/`, or trigger the failsafe mechanism by immediately powering off the Kobo after it starts booting.

Most errors, if any, will be displayed as a menu item in the main menu. If no new menu entries appear here after a reboot, try reinstalling NickelMenu. If that still doesn't work, connect over telnet or SSH and check the output of `logread`.

## Compiling

Build with the public [kobuild](https://github.com/nicoverbruggen/kobuild) SDK images. `tools/build.sh` pins both Qt images from `v0.5.3` by digest. The images provide the compiler, sysroot and Qt; no local SDK or Qt build is needed. Docker selects the image for the host's AMD64 or ARM64 architecture.

```sh
git submodule update --init
sh tools/build.sh
```

Use `sh tools/build.sh --qt 6` or `--qt 5` to build one target. Set `CONTAINER_ENGINE=podman` to use Podman and `JOBS` to change the default of two compiler jobs. Each target builds a fresh source copy with the pinned NickelHook dependency. Outputs are `build/qt5/KoboRoot.tgz` and `build/qt6/Kobo.tgz`, with the corresponding `libnm.so` beside each package. Builds check archive paths, permissions, ARM hard-float ABI and Qt dependencies before copying the output.

The version comes from `git describe --tags --always --dirty`. Use `--version VERSION` to override it for a local test build. GitHub Actions builds both targets. The manual Release workflow builds an existing tag, runs the host tests, checks both packages and attaches them to one draft release. Tags starting with `qt6-` are marked as prereleases. Publish only after validating the packages on the intended firmware.

Run the host action, parser and symbol-checker tests with `cd test/syms && go test ./...` using the Go version in `go.mod`. These tests and the package checks do not establish device compatibility.

## Qt6 support and builds

This branch adds experimental Qt6 support for Kobo firmware 5.18 and 6.0. Qt5 remains the default for direct Makefile builds on firmware 4.x; the two Qt versions require separate binaries. For a direct build inside a compatible SDK:

With that toolchain on `PATH`:

```sh
make NH_QT_MAJOR=6 clean
make NH_QT_MAJOR=6 NM_UNINSTALL_CONFIGDIR=1 -j2 all
make NH_QT_MAJOR=6 kobo
```

Install the resulting `Kobo.tgz` through `.kobo` on Qt6 firmware. It contains only the plugin; documentation is created on user storage at first startup. Qt5 continues to use `KoboRoot.tgz`. To build against another NickelHook checkout, set `NICKELHOOK=/path/to/NickelHook`.

The Qt6 port adapts selection menus and actions to the changed firmware APIs and checks private dependencies before using them. Extras, Bluetooth, Pocket and awards are unavailable in this port. Physical hardware actions still need device testing.

Qt6 actions resolve available firmware symbols without requiring exact machine-code hashes. The runtime still checks executable memory, native Qt allocation metadata and the Settings vtable slots it replaces. Navigation errors distinguish missing symbols from rejected dependencies. The symbol checker reports changed function fingerprints for review without rejecting a Qt6 rebuild. These checks do not prove private object-layout compatibility; new firmware still needs runtime testing.
