# river-shifttags

A small utility for the river Wayland compositor to rotate the focused tags.
Useful for focusing next/prev tag, or rotating the whole tagmask if multiple
tags are in focus.

# Installation

```sh
$ make
# make install
```

# Usage

To rotate the currently focused once to the right
```sh
river-shifttags
```

To rotate the currently focused once to the left
```sh
river-shifttags --shifts -1
```

To rotate a different number of tags
```sh
river-shifttags --num-tags 16
```

## Example configuration

```sh
riverctl map normal $super BRACKETRIGHT spawn  'river-shifttags'
riverctl map normal $super BRACKETLEFT spawn  'river-shifttags --shift -1'
```

# Contributing

Please feel free to submit a merge request if you find something to improve in
the code. Come across an issue? please report it. 

tl;dr contributions welcome.


# License

GPLv3
