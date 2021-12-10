#!/bin/sh
## simet_geolocation_legado.sh - simet geolocation helper (legacy)
## Copyright 2012-2019 NIC.br
##
## Parameters:
## $1 - measure id, optional. When unspecificed, does only
##      device geolocation webservice update calls.
##
## Options (must come before the first parameter)
## --max-age #, --fail-silently : passed as-is to simet_geolocation.sh
##
## calls into simet_geolocation.sh (and/or uses its cache)
## for the real geolocation work.
##
## simet_geolocation.sh API level: 1

set -e
set -o pipefail

TMPGEO=
TMPGEOD=
abend() {
  [ -n "$TMPGEO" ] && rm -f "$TMPGEO" 2>/dev/null
  [ -n "$TMPGEOD" ] && rm -f "$TMPGEOD" 2>/dev/null
  echo "$0: error: $*" >&2
  exit "$RC"
}
soft_abend() {
  [ -n "$TMPGEO" ] && rm -f "$TMPGEO" 2>/dev/null
  [ -n "$TMPGEOD" ] && rm -f "$TMPGEOD" 2>/dev/null
  [ -n "$FAIL_SILENTLY" ] && exit 0
  echo "$0: error: $*" >&2
  exit "$RC"
}
log() {
  echo "$0: $*" >&2
  :
}

# for old API data
. /etc/simet/simet1.conf

. /usr/lib/simet/simet_lib.sh
[ -z "$GEOLOC_CACHE" ] && abend "GEOLOC_CACHE missing from conf file"
FAIL_SILENTLY=
MAX_AGE=
RC=1

# Handle command line
while [ $# -gt 0 ] ; do
  case "$1" in
    --fail-silently)
      FAIL_SILENTLY="--fail-silently"
      ;;
    --max-age)
      shift
      MAX_AGE="--max-age $1"
      ;;
    *)
      hash_measure="$1"
      ;;
  esac
  shift
done

# simplified, assumes simet_geolocation.sh did most of the work
simet_read_geo_cache() {
  [ -r "${GEOLOC_CACHE}" ] || return 1;
  GEO_UTIME=$(sed -n -e '1 { p ; q }' "${GEOLOC_CACHE}") || return 1
  [ -z "$GEO_UTIME" ] && return 1
  GEO_LAT=$(sed -n -e '2 {s/ .*// ; p ; q}' "${GEOLOC_CACHE}")
  [ -z "$GEO_LAT" ] && return 1
  GEO_LNG=$(sed -n -e '2 {s/^ *[^ ]\+ *// ; s/ *[^ ]\+ *$// ; p ; q}' "${GEOLOC_CACHE}")
  [ -z "$GEO_LNG" ] && return 1
  GEO_ACC=$(sed -n -e '2 {s/^ *[^ ]\+ \+[^ ]\+ *// ; p ; q}' "${GEOLOC_CACHE}")
  [ -z "$GEO_ACC" ] && return 1
  :
}

##
## legacy SIMET-1 API
##

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
## MAIN
##

# (try to) geolocate when required
# we will read the result from the cache (easier than parsing LMAP tables)
simet_geolocation.sh $FAIL_SILENTLY $MAX_AGE >/dev/null || exit $?

simet_read_geo_cache || RC=1 soft_abend "No valid geolocation cached data found"

hash_device=${hash_device:-$(get_mac_address.sh)}

if [ -z "$hash_measure" ] ; then
	echo "hash_measure not specificed, will persist only device location" >&2
fi

TMPGEO=$(mktemp -t simetgeoloc.$$.XXXXXX) && TMPGEOD=$(mktemp -t simetgeoloc_d.$$.XXXXXX)

if [ -z "$TMPGEO" ] || [ -z "$TMPGEOD" ] ; then
	[ -n "$TMPGEO" ] && rm -f "$TMPGEO"
	[ -n "$TMPGEOD" ] && rm -f "$TMPGEOD"
	exit 3
fi

generate_simetws_device "$GEO_LAT" "$GEO_LNG" "$GEO_ACC"  > "$TMPGEOD"
envia_geolocation_d=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence_optional/geolocation-device" -f "$TMPGEOD")

if [ -n "$hash_measure" ] ; then
	generate_simetws_measure "$GEO_LAT" "$GEO_LNG" "$GEO_ACC" > "$TMPGEO"
	envia_geolocation=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence/geolocation" -f "$TMPGEO")
else
	envia_geolocation="(skipped, hash_measure was not specificed)"
fi

rm -f "$TMPGEO"
rm -f "$TMPGEOD"

echo "SIMET geoapi: measurement location result: $envia_geolocation" >&2
echo "SIMET geoapi: device location result: $envia_geolocation_d" >&2

exit 0
