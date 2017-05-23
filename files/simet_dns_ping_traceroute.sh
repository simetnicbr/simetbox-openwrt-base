#!/bin/sh

# inicia a variaveis
status=0
tmpfile=$(mktemp -t "simet_dns_ping_traceroute.XXXXXX")

# parametros

simet_dns_ping_traceroute > $tmpfile
while read line
do
	lineping=`echo $line | awk '{printf "-i "; for(i=1;i<=4;i++) printf $i OFS; for(i=7;i<=NF;i++) printf $i OFS; printf ORS}'`
	simet_ping.sh  $lineping
	simet_traceroute.sh $line
done <$tmpfile


# remove arquivos temporarios
for i in "$tmpfile"
do
	if [ -e "$i" ]; then
		echo removing $i >&2
		rm $i
	fi
done

exit $status
