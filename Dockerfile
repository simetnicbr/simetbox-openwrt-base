# Base image
FROM debian:latest
MAINTAINER "Fabio Rodrigues Ribeiro <farribeiro@gmail.com>"

ADD https://github.com/simetnicbr/simetbox-openwrt-base/archive/master.tar.gz /src/simet-openwrt-base.tar.gz

WORKDIR "/src"

RUN	apt-get update \
	&& apt-get upgrade -y \
	&& apt-get install -y \
	build-essential \
	dh-autoreconf \
	libssl-dev \
	libjson0 \
	libjson0-dev \
	ibncurses5-dev \
	unzip \
	gawk \
	zlib1g-dev \
	subversion \
	--no-install-recommends \
	&& tar zxf simetbox-openwrt-base.tar.gz

WORKDIR "/src/simetbox-openwrt-base-master"

RUN	autoreconf ; exit 0

RUN	automake --add-missing \
	&& ./configure \
	&& make install

ADD	https://github.com/openwrt/archieve/master.tar.gz /src/openwrt.tar.gz

