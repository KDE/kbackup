#!/bin/sh
# kbackup script to do a rotating backup (keep only a defined number of backups)

NUM_BACKUPS=20

mode=$1
archive=$2
target=$3
mountPoint=$4

case "$mode" in
 "slice_init" )
 ;;

 "slice_closed" )
 ;;

 "slice_finished" )
   count=`ls -A1 $target | wc -l`
   if [ $count -ge $NUM_BACKUPS ]
   then
     removeCount=`expr $count - $NUM_BACKUPS`
     oldest=`ls -A1 ${target}/* | sort | head -n $removeCount`
     rm -f ${oldest}
   fi
 ;;
esac
