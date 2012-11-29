fuse-simple-cow
===============

All cows are simple.

A very (very) simple copy-on-write file system using FUSE. The main use is to reduce
space usage in duplicated directory trees (i.e. for vservers).

Usage
=====
```bash
$ cp -rl base/ copy/
$ ./simple-cow mnt -o src_dir=base -o cow_dir=copy
```

mnt will be a mirror of copy, however, write operations inside mnt will not affect files
in base.