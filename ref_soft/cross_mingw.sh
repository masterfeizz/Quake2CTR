#!/bin/sh

. ../cross_defs.mingw

if test "$1" = "strip"; then
	echo $TARGET-strip ref_soft.dll
	$TARGET-strip ref_soft.dll
	exit 0
fi

exec make CC=$TARGET-gcc -f Makefile.mingw $*
