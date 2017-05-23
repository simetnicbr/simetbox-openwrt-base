#!/bin/sh

. /etc/config/simet.conf
param_hash_measure=0
tem_nome=0
tem_origem=0
familia=""
uso="uso: simet_traceroute [IP_DESTINO].\n\nOpcoes\n\t-m [HASH] recebe o hashmeasure a ser usado\n\t-n [NOME_DESTINO] do servidor testado.\n\t-o [IP_ORIGEM] IP da maquina que gerou "
while getopts "46hm:n:o:" optname; do
	case "$optname" in
		"4")
			echo "opcao familia $optname" >&2
			familia="-4"
			;;
		"6")
			echo "opcao familia $optname" >&2
			familia="-6"
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
		"o")
			echo "opcao $optname = $OPTARG" >&2
			origem=$OPTARG
			tem_origem=1
			;;
		"?")
			echo "opcao desconhecida $OPTARG" >&2
			;;
		"h")
			echo -e "$uso" >&2
			;;
		":")
			echo "$OPTARG chamado sem opcao" >&2
			;;
		*)
			echo "erro desconhecido" >&2
			;;
	esac
done
shift $((OPTIND-1))
if [ "$1" = "" ]; then
	echo $uso >&2
	exit
else
	echo testando rota para $1. Aguarde... >&2
	ip=$1
fi

if [ "$familia" = "" ]; then
	if [ `echo $ip|grep ":"|wc -l` -eq 0 ]; then
		familia="-4"
	else
		familia="-6"
	fi
fi
if [ $tem_origem -eq 0 ]; then
	origem=`simet_ws $familia https://$cf_host/$cf_simet_web_services/IpClienteServlet`
fi

if [ "$origem" = "" ]; then
	echo "sem IP de origem no protocolo $familia"
	exit
fi
# inicia a variaveis
sendfile=$(mktemp -t "simet_traceroute_envia.XXXXXX")
tmpfile=$(mktemp -t "simet_traceroute.XXXXXX")
tmpfile2=$(mktemp -t "simet_traceroute.XXXXXX")

traceroute -I -n $ip >$tmpfile
# testo o resultado do traceroute
if [ $? -eq 0 ]; then
	# sucesso
	#obs.: o "if ($i ~ /[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/ || $i ~/\:/)" identifica a string de um IPv4 ou IPv6 na saída do traceroute -- SÓ FUNCIONA na saída do traceroute. Não é genérico.
	grep -v "traceroute" <$tmpfile | sed 's/ms//g;s/*//g;s/!A//g;s/!C//g;s/!F//g;s/!H//g;s/!N//g;s/!P//g;s/!Q//g;s/!S//g;s/!T//g;s/!U//g;s/!X//g;s/!Z//g;'| awk '{hopstep=1; for(i=2;i<=NF;i++) {if ($i ~ /[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/ || $i ~/\:/) ip_route=i; else { print $1, $ip_route, $i, hopstep; hopstep++}}}' >$tmpfile2
fi


# formata json
#pega hash_measure
if [ $param_hash_measure -eq 0 ]; then
	hash_measure=`simet_hash_measure -s $cf_host`
fi
if [ "$hash_measure" = "" ]; then
	echo nao conseguiu obter hash_measure >&2
	exit
else
	echo hashMeasure: [$hash_measure]>&2
fi



if [ $tem_nome -eq 1 ]; then
	echo {\"hashMeasure\":\"$hash_measure\",\"nomeDestino\":\"$nome\",\"ipDestino\":\"$1\",\"ipOrigem\":\"$origem\",\"traceroute\":[ >$tmpfile
else
	echo {\"hashMeasure\":\"$hash_measure\",\"ipDestino\":\"$1\",\"ip_origem\":\"$origem\",\"traceroute\":[ >$tmpfile
fi

awk '{print "\x7b\x22""host\x22:\x22"$2"\x22,\x22idDirection\x22:\x22"2"\x22,\x22timeMs\x22:\x22"$3"\x22,\x22hop\x22:"$1",\x22hopStep\x22:"$4"\x7d,"}' < $tmpfile2 |sed '$s/.$//' >>$tmpfile
echo ]} >>$tmpfile
tr -d "\\n"  <$tmpfile >$sendfile
#envia sendfile
saida=`simet_ws -p https://$cf_host/$cf_simet_web_persistence/traceroute -f $sendfile`
if [ "$saida" = "true" ]; then
#se enviou corretamente, apaga sendfile
	echo enviou corretamente
else
	echo nao conseguiu enviar. resultado: [$saida]
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

