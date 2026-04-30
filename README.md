<h1 align="center">NickelMenu</h1>

The easiest way to launch custom scripts, change hidden settings, and run actions on Kobo eReaders.

See the [website](https://pgaskin.net/NickelMenu) and [thread on MobileRead](https://mobileread.com/forums/showthread.php?t=329525) for screenshots and more details.

Firmware 5.x is not supported yet.

## This Fork

This fork adds a small set of Kobo home screen customizations and related debugging on top of upstream NickelMenu.

The main functional change is support for experimental options to hide specific home screen widgets by their internal object name. The currently documented options are `experimental:hide_home_row1col2_enabled:1`, `experimental:hide_home_row2col2_enabled:1`, and `experimental:hide_home_row3_enabled:1`, which hide parts of the home screen layout after `HomePageView` is constructed. These options are all constrained within `mainContainer`, with `row1col2` resolved under `row1`, `row2col2` resolved under `row2`, and `row3` hidden as its own section, so other views reusing the same leaf object names are not hidden accidentally. The `row2col2` option uses a visual-only hide so the layout does not collapse.

This branch also adds extra `HomePageView` debug logging to help identify widgets for further tweaks. When the home page view is constructed, it logs the number of child `QWidget` instances and prints them as a tree, including each widget's class name, object name, and pointer.

In addition, the fork includes updated symbol test coverage for firmware `4.45.23646`.

## Installation
You can download pre-built packages of the latest stable release from the [releases](https://github.com/pgaskin/NickelMenu/releases) page, or you can find bleeding-edge builds of each commit from [here](https://github.com/pgaskin/NickelMenu/actions).

After you download the package, copy `KoboRoot.tgz` into the `.kobo` folder of your eReader, then eject it.

After it installs, you will find a new menu item named `NickelMenu` with further instructions which you can also read [here](./res/doc).

To uninstall NickelMenu, just create a new file named `uninstall` in `.adds/nm/`, or trigger the failsafe mechanism by immediately powering off the Kobo after it starts booting.

Most errors, if any, will be displayed as a menu item in the main menu. If no new menu entries appear here after a reboot, try reinstalling NickelMenu. If that still doesn't work, connect over telnet or SSH and check the output of `logread`.

## Compiling

NickelMenu is designed to be compiled with [NickelTC](https://github.com/pgaskin/NickelTC). To compile it with Docker/Podman, use `docker run --volume="$PWD:$PWD" --user="$(id --user):$(id --group)" --workdir="$PWD" --env=HOME --entrypoint=make --rm -it ghcr.io/pgaskin/nickeltc:1.0 all koboroot`. To compile it on the host, use `make CROSS_COMPILE=/path/to/nickeltc/bin/arm-nickel-linux-gnueabihf-`.

For a local Podman build wrapper, run `./build.sh`. It defaults to `clean all koboroot`, and you can pass alternate make targets directly, such as `./build.sh all` or `./build.sh clean koboroot`. The wrapper uses `--userns=keep-id` so rootless Podman can write build outputs back into the working tree correctly.

<!-- TODO: a lot more stuff -->
