#!/bin/sh

. /etc/config/simet.conf

. /usr/bin/test_runner.sh

TestSNMP()
{
	family=$1
	server=$2

	simet_snmp $family -s $server
}

RunTest 'TestSNMP'