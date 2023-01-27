# noupstate - nouveau reclocking utility

 A simple utility for changing nouveau pstates.

## Warning

This is a quick utility that works fine on my system. It may cause unintended effects on your system or even damage your hardware.  
Check your GPU temperatures frequently!
  
**USE AT YOUR OWN RISK!**

## Usage

`noupstate` needs elevated permissions.

- `noupstate list` lists all avaliable pstates for device 0. To specify any other device, use the `-d <device>` option.
- `noupstate set -d performance | save-energy | id:<id> | val:<value>` sets a specific pstate for device 0. To specify any other device, use the `-p <device>` option.  
  Note that this will only last until you reboot your system.

## Installation

Clone this repository and `cd` into it.  
Compile with `make`. The binary will be generated in the `bin/` directory.  
You can install the binary with `make install` (optional).

```sh
git clone *link*
cd noupstate
make
sudo make install # optional, installs the binary to your system.
```

