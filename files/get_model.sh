#!/bin/sh

. /lib/functions.sh

machine=$(awk 'BEGIN{FS="[ \t]+:[ \t]"} /machine/ {print $2}' /proc/cpuinfo | sed -e 's/ //g')
echo $machine
