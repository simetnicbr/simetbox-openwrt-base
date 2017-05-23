#!/bin/sh

uci set $1.$2=$3
uci commit $1

