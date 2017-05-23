#!/bin/sh

. /etc/config/simet.conf
# inicia a variaveis
zipdate="/tmp/simet_ping.date"
zipfile="/tmp/simet_ping.gz"
sendfile="/tmp/envia.json"
tmpfile=$(mktemp -t "simet_ping.XXXXXX")
tmpfile2=$(mktemp -t "simet_ping.XXXXXX")
gw4=`netstat -nr | grep "^0.0.0.0" |head -n1 |awk '{print $2}'`
ifacev4=`netstat -nr | grep "^0.0.0.0" |head -n1 |awk '{print $8}'`
gw6=`route -A inet6 |grep UG |head -n1 | awk '{print $2}'`
ifacev6=`route -A inet6 |grep UG |head -n1 | awk '{print $7}'`
mac=`get_mac_address.sh`
count=3
w=$(($count+10))
individual=0
tem_nome=0
param_hash_measure=0
familia=""
nao_pingou=1
# parametros
while getopts "46c:im:n:W:h" optname; do
	case "$optname" in
		"4")
			echo "opcao familia $optname" >&2
			familia="-4"
			;;
		"6")
			echo "opcao familia $optname" >&2
			familia="-6"
			;;
		"c")
			echo "opcao $optname = $OPTARG" >&2
			count=$OPTARG
			;;
		"i")
			echo "opcao $optname" >&2
			individual=1
			;;
		"m")
			echo "opcao $optname = $OPTARG" >&2
			hash_measure=`echo $OPTARG | awk '{if ($0 ~ /MEASURE[0-9A-Za-z]+/) print $0}'`
			param_hash_measure=1
			;;
		"n")
			echo "opcao $optname = $OPTARG" >&2
			nome=$OPTARG
			tem_nome=1
			;;
		"W")
			echo "opcao $optname = $OPTARG" >&2
			w=$OPTARG
			;;
		"h")
			echo -e "uso simet_ping [IP] \n\nOpcoes:\n\t-4 familia ipv4\n\t-6 familias ipv6\n\t-c [VALOR] quantidade de pacotes ping\n\t-i teste individual (nao armazena resultados para envio posterior em caso de estar offline)\n\t-W [VALOR] tempo maximo para aguardar respostas do ping.\n\tCaso o IP seja omitido, sera' feito o ping para o gateway padrao." >&2
			;;
		"?")
			echo "opcao desconhecida $OPTARG" >&2
			exit
			;;
		":")
			echo "$OPTARG chamado sem opcao" >&2
			exit
			;;
		*)
			echo "erro desconhecido" >&2
			exit
			;;
	esac
done
shift $((OPTIND-1))

for proto in "-4" "-6"; do
	if [ "$proto" = "-4" ]; then
		gw=$gw4
		iface=$ifacev4
	else
		gw=$gw6
		iface=$ifacev6
	fi
	if [ "$gw" = "" ]; then
		echo sem gw para $proto >&2
		continue;
	fi
	if [ "$proto" = "-4" -a "$familia" = "-6" ]; then
		echo nao faz ping $proto >&2
		continue;
	fi
	if [ "$proto" = "-6" -a "$familia" = "-4" ]; then
		echo nao faz ping $proto >&2
		continue;
	fi


	if [ "$*" = "" ]; then
		ip=$gw
	else
		ip=$*
	fi

	if [ $individual -eq 1 ]; then
		sendfile=$(mktemp -t "simet_ping.XXXXXXX")
	else
		sendfile="/tmp/envia.json"
	fi

	#echo parametros: ip = $ip count = $count wait = $w individual = $individual fecha>&2



	epoch=`date +%s`
	# obtem data arquivo com resultados
	if [ $individual -eq 0 ]; then
		if [ -e "$zipdate" ]; then
			echo encontrou zipdate >&2
			data_arq=`date -r $zipdate`
			epoch_arq=`date -r $zipdate +%s`
			zcat $zipfile >$tmpfile2
		else
			# primeiro teste
			echo primeiro teste >&2
			touch $zipdate
			data_arq=`date`
			epoch_arq=$epoch
		fi
	fi
	# preenche cabecalho teste
	echo $gw $ip $epoch $count| tr -d "\\n" >>$tmpfile2
	ping $proto -I $iface -W $w -c $count $ip >$tmpfile
	# testo o resultado do ping
	if [ $? -eq 0 ]; then
		# pingou
		grep ttl <$tmpfile | sed 's/icmp_req/num/g;s/seq/num/g' | awk '{print $5 $6 $7}'|sed 's/num=/ /g;s/ttl=/ /g;s/time=/ /g' | tr -d "\\n">>$tmpfile2
		nao_pingou=0
		echo pingou com sucesso >&2
	else
		nao_pingou=1
		echo nao conseguiu pingar >&2
	fi
	echo "">>$tmpfile2

	# expurgo - remove testes com mais de uma semana de idade
	awk -v a="$epoch" '{if ($2 >= a - 60*60*24*7)print;}' <$tmpfile2 >$tmpfile

	# zipa
	if [ $individual -eq 0 ]; then
		gzip -c $tmpfile >$zipfile
	fi


	# formata json
	echo formata json >&2
	echo {\"hash_device\":\"$mac\",\"device_type\":\"SIMET-BOX\", >$tmpfile2


	#se tem hash_measure na linha de comando
	if [ $param_hash_measure -eq 1 ]; then
		echo \"hash_measure\":\"$hash_measure\", >>$tmpfile2
	fi
	echo \"simet_ping\":[ >>$tmpfile2

	#se tem nome na linha de comando
	#if [ $tem_nome -eq 1 ]; then
	#	echo \"nome\":\"$nome\", >>$tmpfile2
	#fi


	awk '{print "{\x22""default_gateway\x22:\x22"$1"\x22,\x22""destination\x22:\x22"$2"\x22,\x22""date\x22:"$3",\x22""count\x22:"$4",\x22ping\x22:["};{if (NF>4) for(i=5;i<=NF-3;i=i+3) print "\x7b\x22""num\x22:"$i",\x22""ttl\x22:"$(i+1)",\x22""time\x22:"$(i+2)"\x7d,"}; {if (NF>4) print "\x7b\x22""num\x22:"$(NF-2)",\x22""ttl\x22:"$(NF-1)",\x22""time\x22:"$NF"\x7d]\x7d,"; else print "]\x7d,";}' < $tmpfile | sed 's/{"num":,"ttl":,"time":},//g;s/{"num":,"ttl":,"time":}//g' |sed '$s/.$//'>>$tmpfile2
	echo ]} >>$tmpfile2
	tr -d "\\n"  <$tmpfile2 >$sendfile

	#envia sendfile
	saida=`simet_ws -p https://$cf_host/$cf_simet_web_persistence/ping -f $sendfile`
	if [ "$saida" = "true" ]; then
	#se enviou corretamente, apaga sendfile
		echo enviou resultado do teste corretamente >&2
		for i in "$sendfile" "$zipdate" "$zipfile"
		do
			if [ -e "$i" ]; then
				echo removing $i >&2
				rm $i
			fi
		done
	else
		echo nao conseguiu enviar resultado do teste. resultado: [$saida] >&2
		cat $sendfile
	fi


	# remove arquivos temporarios
	for i in "$tmpfile" "$tmpfile2" "$sendfile"
	do
		if [ -e "$i" ]; then
			echo removing $i >&2
			rm $i
		fi
	done
done

exit $nao_pingou
