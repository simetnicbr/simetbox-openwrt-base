#!/bin/sh

. /lib/functions.sh

# Works well on ar71xx/generic
machine=$(sed -n -e '/^machine[[:blank:]]/ { s/[^:]\+:[[:blank:]]\+// ; s/[ ,;#$%&\!/]/_/g ; s/_\+/_/g ; p }' < /proc/cpuinfo)

# shortcut, since we will need to load in json support otherwise
if [ -n "$machine" ] ; then
	echo $machine
	exit 0
fi

# do it the hard (and expensive) way
. /usr/share/libubox/jshn.sh
json_init
json_load "$(ubus call system board)" || {
	echo "unknown"
	exit 0
}
json_get_var boardname board_name
json_select release
json_get_var target target

if [ x"${target}${boardname}" != x ] ; then
	machine=$(echo "${target}_${boardname}" | sed -e 's/[ ,;#$%&\!/]/_/g' -e 's/_\+/_/g')
else
	# catch-all, because things just don't work well when it is empty
	machine="unknown"
fi

echo $machine
