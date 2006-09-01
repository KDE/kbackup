#!/bin/sh

mode=$1
archive=$2
target=$3
mountPoint=$4

case "$mode" in
 "slice_init" )
    if [ "$mountPoint" != "" ]
    then
      mount /media/zip
      rm -f /media/zip/backup_2*.tar*
    fi
 ;;

 "slice_closed" )
 ;;

 "slice_finished" )
    if [ "$mountPoint" != "" ]
    then
      umount /media/zip
      eject /media/zip
    fi
 ;;
esac
