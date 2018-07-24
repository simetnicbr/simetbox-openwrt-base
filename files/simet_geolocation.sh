#!/bin/sh

###
## simet_geolocation.sh - simet geolocation helper
## Copyright 2012-2017 NIC.br
##
## Parameters:
## @1 - measure id, optional. When unspecificed, does only
##      device geolocation webservice update calls.
##

# Pode ser modificado por simet.conf ou simet-private.conf, mas
# isso não é oficialmente documentado.
GEOLOC_DIR=/tmp/simet_geo
GEOLOC_CACHE="${GEOLOC_DIR}/simet_geolocation_cache"
GEOLOC_LIMIT="${GEOLOC_DIR}/simet_geolocation_tries"

. /etc/config/simet.conf
[ -r /etc/config/simet-private.conf ] && . /etc/config/simet-private.conf

[ ! -d "${GEOLOC_DIR}" ] && mkdir -p "${GEOLOC_DIR}"

#
# Chave de API para geolocalização é requisito
# para que este script funcione.  Caso não tenha
# sido definida, sai sem fazer nada.  Defina em
# /etc/config/simet-private.conf.
#

if [ -z "$GOOGLE_MAP_GEOLOC_APIKEY" ] ; then
	echo "$0: please define GOOGLE_MAP_GEOLOC_APIKEY in /etc/config/simet-private.conf" >&2
	exit 0
fi

hash_device=${hash_device:-$(get_mac_address.sh)}

##
# persist_geoloc_RAM() - armazena em cache volátil local a geoloc
# $1: latitude
# $2: longitude
# $3: precisao
#
persist_geoloc_RAM() {
	# Presença ou ausência do arquivo de cache indica se alguma
	# geolocalização já foi feita neste boot.
	touch "${GEOLOC_CACHE}" && chmod 0600 "${GEOLOC_CACHE}"
	( date -u +%s && echo "$1" "$2" "$3" ) > "${GEOLOC_CACHE}"
	:
}

##
# log_daily_geolocs() - log we did a geoloc today
#
log_daily_geolocs() {
	date -u +%Y%m%d >> "${GEOLOC_LIMIT}"
}

##
# limit_daily_geolocs() - limita geolocations/dia
#
# retorna 0 se atingiu limite de geolocalizacoes
#
limit_daily_geolocs() {
	[ -r "${GEOLOC_LIMIT}" ] || return 1

	today=$(date -u +%Y%m%d)
	todc=$(grep -c "${today}" "${GEOLOC_LIMIT}" 2>/dev/null || echo 0)

	if [ "$(head -n 1 ${GEOLOC_LIMIT})" != "${today}" ] ; then
		rm -f "${GEOLOC_LIMIT}"
		return 1
	fi
	if [ ${todc} -le 2 ] ; then
		return 1
	fi

	return 0
}

##
# cache_not_too_old - returns 0 if it is not too old
#
cache_not_too_old() {
	[ -r "${GEOLOC_CACHE}" ] || return 1;

	cache_time=$(head -n 1 "${GEOLOC_CACHE}")
	# Shell is limited to 32bits, not y2038-safe, but we don't have bc :(
	time_delta=$(( $(date +%s -u) - cache_time ))
	if [ $time_delta -ge 7200 ] ; then
		# more than two hours

		# can we geolocate again today?
		if limit_daily_geolocs ; then
			#no, use old cache
			return 0;
		fi
		return 1;
	fi

	return 0;
}

##
##
# generate_geopost_body() - gera POST body para Google geolocation API
# $@ : endereços BSS nas redondezas
#
# Gera POST body segundo https://developers.google.com/maps/documentation/geolocation/intro
#
# Retorna: <stdout> - POST body
#          $?       - 0 se ok, errno em outro caso.
generate_geopost_body() {
cat <<EOF
{ "considerIp": "false", "wifiAccessPoints": [
EOF

while [ $# != 0 ] ; do
	mac=$1
	shift

	if [ $# != 0 ] ; then
		echo "{ \"macAddress\": \"$mac\" },"
	else
		echo "{ \"macAddress\": \"$mac\" }"
	fi
done

cat <<EOF2
] }
EOF2
}

##
# send_geoquery() - envia query de geolocation
# $1      : chave de API
# <stdin> : body da requisição
#
# Retorna:
# <stdout> : resultado da query
# $?       : 0 se ok, errno em outro caso
send_geoquery() {
	curl -s -d @- -H "Content-type: application/json" -i "https://www.googleapis.com/geolocation/v1/geolocate?key=$1" || {
		echo "CURL error, could not contact geoapi server" >&2
	}
	:
}

##
# generate_simetws_measure() - cria body georequest measure
# $1     : latitude
# $2     : longitude
# $3     : accuracy
generate_simetws_measure() {
echo "{
\"hashMeasure\":\"$hash_measure\",
\"geolocation\":[
{
\"longitude\": $2,
\"latitude\": $1,
\"precisionMeters\": $3,
\"idGeolocationSource\": 2
}
]
}"
}

##
# generate_simetws_device() - cria body georequest dispositivo
# $1     : latitude
# $2     : longitude
# $3     : accuracy
generate_simetws_device() {
echo "{
\"hashDevice\":\"$hash_device\",
\"geolocation\":
{
\"longitude\": $2,
\"latitude\": $1,
\"precisionMeters\": $3,
\"idGeolocationSource\": 2
}
}"
}

##
# geoapi_precheck() - testes de prelaunch dos BSS
# $1..$n : enderecos BSS
#
# Testes de prelaunch, exit se nao adianta fazer a
# query da geoapi
geoapi_precheck() {
	# no minimo 2 BSS sao necessarios
	if [ $# -le 2 ] ; then
		echo "Not enough BSS addresses to geo-locate" >&2
		exit 0
	fi
	:
}

###
### MAIN
###

# Pode ser vazia!
hash_measure=$1

if [ -z "$hash_measure" ] ; then
	echo "hash_measure not specificed, will persist only device location"
fi

if cache_not_too_old ; then
	# return from cache

	echo "Using cached geolocation..."
	saida_geoloc=$(tail -n 1 "${GEOLOC_CACHE}")
	echo "geolocation result (cached): $saida_geoloc"

	if [ -z "${saida_geoloc}" ] ; then
		echo "error: geolocation cache corrupted, removing..." >&2
		rm -f "${GEOLOC_CACHE}"
		exit 0
	fi
else
	# GEOLOCATE

	#FIXME: no lugar de ligar "oficialmente" o radio de forma temporária,
	#ligar direto em modo passivo, fazer o scan, e derrubar, assim não
	#corre o risco dele associar, receber IP, etc.
	total_radio=$(uci show wireless | awk -F '.' '/=wifi-device/ {print $2}' | wc -l)
	mac_address=""
	cont=0
	while [ $cont -lt $total_radio ]
	do
		echo "cont: $cont"
		radio_state_disabled=$(uci -q get wireless.radio$cont.disabled)
		radio_state_disabled=${radio_state_disabled:-0}
		echo "radio_state_disabled: $radio_state_disabled"
		if [ $radio_state_disabled == 1 ] ; then
			uci set wireless.radio$cont.disabled=0
			wifi
			sleep 10
		fi
		scan=
		for i in 1 2 3 4 5; do
			[ -z "$scan" ] && scan=$(iw wlan$cont scan | awk '/^BSS / {print $2}' | sed -e 's/(on//')
			[ -z "$scan" ] && sleep 5
		done
		if [ $radio_state_disabled == 1 ] ; then
			uci set wireless.radio$cont.disabled=1
			wifi
		fi
		mac_address="$mac_address
$scan"
		cont=$(expr $cont + 1)
	done

	echo "BSSes detected for geolocalization: $mac_address"

	geoapi_precheck $mac_address

	saida_geoloc=$(generate_geopost_body $mac_address | send_geoquery "$GOOGLE_MAP_GEOLOC_APIKEY" | \
		awk '
			BEGIN { FS=":|," ; accuracy=0 ; code=200 ; n=0 }
			/"code"/     { code=$2 }
			/"accuracy"/ { accuracy=$2 }
			/"lat"/      { lat=$2 ; n++ }
			/"lng"/	     { lng=$2 ; n++ }
			END { if (n == 2 && code == 200) print lat lng accuracy }
		')

	if [ "x$saida_geoloc" = "x" ] ; then
		# Problema ao contactar o google, aborta sem sinalizar erro
		echo "geolocation API call failed" >&2
		exit 0
	fi

	echo "geolocation result: " $saida_geoloc
	persist_geoloc_RAM $saida_geoloc
	log_daily_geolocs
fi

TMPGEO=$(mktemp -t simetgeoloc.$$.XXXXXX) && TMPGEOD=$(mktemp -t simetgeoloc_d.$$.XXXXXX)

if [ -z "$TMPGEO" ] || [ -z "$TMPGEOD" ] ; then
	[ -n "$TMPGEO" ] && rm -f "$TMPGEO"
	[ -n "$TMPGEOD" ] && rm -f "$TMPGEOD"
	exit 1
fi

generate_simetws_device  $saida_geoloc > "$TMPGEOD"
envia_geolocation_d=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence_optional/geolocation-device" -f "$TMPGEOD")

if [ -n "$hash_measure" ] ; then
	generate_simetws_measure $saida_geoloc > "$TMPGEO"
	envia_geolocation=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence/geolocation" -f "$TMPGEO")
else
	envia_geolocation="(skipped, hash_measure was not specificed)"
fi

rm -f "$TMPGEO"
rm -f "$TMPGEOD"

echo "SIMET geoapi: measurement location result: " $envia_geolocation
echo "SIMET geoapi: device location result: " $envia_geolocation_d

exit 0
