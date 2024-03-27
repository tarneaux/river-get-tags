# river-get-tags

A small utility to get the currently focused and occupied tags in the River compositor on Wayland.

This will most likely be used for scripting. I used for the my eww (Elkowar's Wacky Widgets) bar.

To configure the number of printed bits, change the `5` value (6 tags minus one for me) in `river-get-tags.c`
```c
void printbits(unsigned int v) {
  int i; // for C89 compatability
  for(i = 0; i <= 5; i++) putchar('0' + ((v >> i) & 1));
}
```

# Installation

```sh
make
sudo make install
```

# Usage

Just run the tool:
```sh
river-get-tags
```

# Credits & License

This tool is heavily based on [Adithya Kumar](https://gitlab.com/akumar-xyz)'s [river-shifttags](https://gitlab.com/akumar-xyz/river-shifttags). I basically just removed some of the functionality and adjusted the prints.

License: GPL version 3
