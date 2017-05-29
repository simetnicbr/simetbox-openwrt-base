# Base image
FROM ubuntu:latest
MAINTAINER "Fabio Rodrigues Ribeiro <farribeiro@gmail.com>"

ADD https://github.com/simetnicbr/simetbox-openwrt-base/archive/master.tar.gz /src/

WORKDIR "/src"

RUN	apt-get update \
	&& apt-get upgrade -y \
	&& apt-get install -y \
	build-essential \
	dh-autoreconf \
	libssl-dev \
	libjson0 \
	libjson0-dev \
	--no-install-recommends \
WORKDIR "/src/simetbox-openwrt-base-master"

	&& autoreconf \
	&& automake --add-missing \
	&& ./configure \
	&& make install
