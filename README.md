# river-get-tags

A small utility to get the currently focused and occupied tags in the River compositor on Wayland.

This will most likely be used for scripting. I used it for my eww (Elkowar's Wacky Widgets) bar.

## Installation

Install dependencies:

- cairo
- wayland-devel
- pkg-config

```sh
make
sudo make install
```

## Usage

The tool provides some options to control its output. However it can be ran without any argument. In this case, it will print focused and occupied tags as a bitfield of the first 6 bits.

The different options of the tool are:

- `-h`: show help message
- `-f`: print the focused tags value
- `-o`: print the occupied tags value
- `-n`: print the name of tags before the value
- `-F`: specify the format used to print the tags value. This option need an argument which can be
  - `%[1-32]b` for printig tags value as bitfield of given length
  - [C printf format](https://cplusplus.com/reference/cstdio/printf/) with (u, o, x, X) specifiers for printing tags value as unsigned integers decimal, octal, lowercase hexadecimal or UPPERCASE HEXADECIMAL

### Examples:

```sh
$ ./river-get-tags # Print focused & occupied tags name & value as 6-bits bitfield
focused: 100010
occupied: 110000

$ ./river-get-tags -fon # Same result as without options
focused: 100010
occupied: 110000

$ ./river-get-tags -fonF%6b # Same result as without options
focused: 100010
occupied: 110000

$ ./river-get-tags -fonF%9b # Printing as 9-bits bitfield
focused: 100010100
occupied: 110000100

$ ./river-get-tags -fonF%u # Same as unsigned decimal integer
focused: 81
occupied: 67

$ ./river-get-tags -fF%u # If we only need the focused tag value
81

$ ./river-get-tags -oF%#x # We can also get only occupied tags value, as an hexadecimal with the '0x' prefix
0x43

$ ./river-get-tags -oF%08x # Or with some padding with '0' char, up to 8-char length
00000043
```

## Credits & License

This tool is heavily based on [Adithya Kumar](https://gitlab.com/akumar-xyz)'s [river-shifttags](https://gitlab.com/akumar-xyz/river-shifttags). I basically just removed some of the functionality and adjusted the prints.

License: GPL version 3
