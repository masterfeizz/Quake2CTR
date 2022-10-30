#!/bin/sh

. ../cross_defs.dj

exec make CC=$TARGET-gcc -f Makefile.dj $*
