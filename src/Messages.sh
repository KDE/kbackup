#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.rc -o -name \*.ui -o -name \*.kcfg` >rc.cpp
$XGETTEXT `find . -name \*.h -o -name \*.cxx -o -name \*.cpp` -o $podir/kbackup.pot
rm -f rc.cpp
