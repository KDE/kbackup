#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.rc -o -name \*.ui -o -name \*.kcfg` >> rc.cpp
$XGETTEXT `find . -name \*.h -o -name \*.\?xx -o -name \*.cpp` -o $podir/kbackup.pot
