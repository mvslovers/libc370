# crent370 libc SDK
#
# Builds crent370 as the cc370 target libc and installs it into the cc370
# sysroot -- pure host toolchain (cc370 -S -> as370 -> ar370), no mbt, no MVS.
# After `make install` the cc370 chain finds crent by itself:
#   cc370 -c foo.c                     # <stdio.h> etc. found with no -I
#   ld370 crt0.o foo.o -lc -o foo.lm   # libc.a + startfile
#
# Artifacts land where cc370 already searches (derived from `cc370 -dumpmachine`
# / -print-prog-name, so a future target-triple rename needs no edit):
#   <sysroot>/include   headers
#   <sysroot>/lib       libc.a + crt0.o crt1.o crtm.o (startup variants)
PY  := python3
SDK := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))mklibc.py

.PHONY: all build install clean
all: install

build:
	$(PY) $(SDK) build

install:
	$(PY) $(SDK) all

clean:
	rm -rf $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../build/sdk
