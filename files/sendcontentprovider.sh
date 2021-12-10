#!/bin/sh
. /etc/simet/simet1.conf

mac_address=`get_mac_address.sh`

family=$1

hash_measure=`simet_hash_measure -s $cf_host`
if [ "$hash_measure" = "" ]; then
        echo nao conseguiu obter hash_measure >&2
        exit 1
fi
echo "Hash Measure: $hash_measure"
ip_facebook=`ping $family -c 1 www.facebook.com | grep PING | awk '{print $3}' | sed -e 's/(//g' | sed -e 's/)//g' | sed s'/.$//'`
local_google=`simet_ws $family https://redirector.c.youtube.com/report_mapping | head -1`
echo "local_google antigo -> $local_google"
local_google=`echo $local_google | sed -e 's/ /%20/g'`
local_google=`echo $local_google | sed -e 's/=/%3D/g'`
local_google=`echo $local_google | sed -e 's/\//%2F/g'`
echo "ip_facebook -> $ip_facebook"
echo "local_google -> $local_google"
string_google="https://$cf_host/$cf_simet_web_services_optional/ContentService?type=GOOGLESERVERLOCATION&hashMeasure=$hash_measure&description=$local_google"
echo "string_google -> \"$string_google\""
result_google=`simet_ws $family $string_google`
echo "result_google -> $result_google"
string_facebook="simet_ws $family https://$cf_host/$cf_simet_web_services_optional/ContentService?type=FACEBOOKSERVERLOCATION&hashMeasure=$hash_measure&description=$ip_facebook"
echo "string_facebook -> $string_facebook"
result_facebook=`$string_facebook`
echo "result_facebook -> $result_facebook"
