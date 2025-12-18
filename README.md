# motd

A lightweight system information fetch tool written in C++, designed for BSD (especially OpenBSD) and Linux systems.

## Features

- **Cross-platform**: Works on Linux and OpenBSD
- **Detailed memory info**: Shows used/total with percentage, plus swap usage
- **Full color palette**: Displays all 8 ANSI colors
- **System information**: OS, kernel, shell, WM/DE, uptime, CPU, packages, terminal, and more
- **Zero dependencies**: Uses only standard C++ and system calls

## Building

This project uses [xmake](https://xmake.io/) as the build system.

### Prerequisites

- C++17 compatible compiler (g++, clang++)
- xmake

### Install xmake

```sh
# On Linux
curl -fsSL https://xmake.io/shget.text | bash

# On OpenBSD
pkg_add xmake
```

### Build

```sh
xmake
```

### Build for release (optimized, stripped)

```sh
xmake f -m release
xmake
```

The binary will be located at `build/<platform>/<arch>/release/motd`

### Install

```sh
xmake install
```

Or manually copy the binary:

```sh
cp build/*/release/motd ~/.local/bin/
# or
sudo cp build/*/release/motd /usr/local/bin/
```

## Usage

Simply run:

```sh
motd
```

Example output:
```
amarnath@enlightenment

      os ~ Gentoo Linux
      sh ~ fish
      wm ~ mango
      up ~ 4h 37m
     gtk ~ adw-gtk3
     box ~ ROG Zephyrus G16 GU605MI_GU605MI
     cpu ~ Intel(R) Core(TM) Ultra 9 185H (22) @ 5.10 GHz
     gpu ~ Intel Corporation Meteor Lake-P [Intel Arc Graphics]
     gpu ~ NVIDIA Corporation AD106M [GeForce RTX 4070 Max-Q / Mobile]
     mem ~ 8352MB / 15380MB (54%)
    swap ~ 735MB / 15379MB (4%)
    disk ~ /: 203.86 GiB / 322.86 GiB (63%) - ext4
    load ~ 2.50, 2.84, 3.33
    kern ~ 6.18.1
    pkgs ~ 1189 (emerge)
    term ~ kitty

  ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅
```

### Shell Integration

Add to your shell rc file (`.bashrc`, `.zshrc`, etc.):

```sh
motd
```

## Supported Systems

- **Linux**: Full support with detailed memory/swap info
- **OpenBSD**: Full support (some features simplified due to platform differences)
- **Other BSDs**: Should work with minimal modifications

## Detected Information

- **OS/Distro**: From `/etc/os-release` or system calls
- **Kernel**: Version from `uname`
- **Shell**: From `$SHELL` environment variable
- **WM/DE**: From environment variables or process scanning
- **Uptime**: System uptime in days, hours, minutes
- **Memory**: Used/Total in MB with percentage
- **Swap**: Used/Total in MB with percentage (if available)
- **CPU**: Processor model
- **Model**: System/motherboard model (Linux only)
- **Packages**: Count from package manager databases
- **Terminal**: Detected from process tree

## License

BSD 3-Clause License - See LICENSE file for details

## Contributing

Contributions welcome! This project aims to be:
- Minimal and fast
- Cross-platform (BSD/Linux focus)
- Zero external dependencies
- Easy to build and deploy
