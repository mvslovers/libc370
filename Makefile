# libc370 -- the C runtime / libc for MVS 3.8j, the cc370 target library.
#
# Builds host-native (cc370 -S -> as370 -> ar370; no mbt, no MVS) and installs
# into the cc370 sysroot, so the toolchain finds it by itself:
#   cc370 -c foo.c                  # <stdio.h> ... found with no -I
#   cc370 foo.c -o foo.xmit         # links libc.a + crt0.o, ready for RECV370
#
#   make            build libc.a + crt0/1/m.o + stage headers & macros
#   make install    install all of it into the cc370 sysroot
#   make clean
PY := python3

.PHONY: all build install clean
all: build
build:
	$(PY) sdk/mklibc.py build
install:
	$(PY) sdk/mklibc.py all
clean:
	rm -rf build/sdk
