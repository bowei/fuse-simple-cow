fuse-simple-cow
===============

All cows are simple.

A very (very) simple copy-on-write file system using FUSE. The main use is to reduce
space usage in duplicated directory trees (i.e. for vservers).

Usage
=====
```bash
$ cp -rl /base /copy
$ fusermount blah blah
```