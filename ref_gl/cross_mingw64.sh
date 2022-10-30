#!/bin/sh

. ../cross_defs.mingw64

if test "$1" = "strip"; then
	echo $TARGET-strip ref_gl.dll
	$TARGET-strip ref_gl.dll
	exit 0
fi

exec make CC=$TARGET-gcc -f Makefile.mingw64 $*
