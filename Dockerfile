# Base image
FROM debian:latest
MAINTAINER "Fabio Rodrigues Ribeiro <farribeiro@gmail.com>"

ADD https://github.com/simetnicbr/simetbox-openwrt-base/archive/master.tar.gz /src/simetbox-openwrt-base.tar.gz
ADD https://github.com/simetnicbr/simetbox-openwrt-feed/archive/master.tar.gz /src/simetbox-openwrt-feed.tar.gz
ADD https://github.com/simetnicbr/simetbox-openwrt-config/archive/master.tar.gz /src/simetbox-openwrt-config.tar.gz
ADD https://github.com/openwrt/openwrt/archive/master.tar.gz /src/openwrt.tar.gz

WORKDIR "/src"

RUN	apt-get update \
	&& apt-get upgrade -y \
	&& apt-get install -y \
	build-essential \
	dh-autoreconf \
	libssl-dev \
	libjson0 \
	libjson0-dev \
	libncurses5-dev \
	unzip \
	gawk \
	zlib1g-dev \
	subversion \
	git \
	ca-certificates \
	--no-install-recommends \
	&& tar zxf simetbox-openwrt-base.tar.gz

WORKDIR "/src/simetbox-openwrt-base-master"

RUN	autoreconf ; exit 0

RUN	automake --add-missing \
	&& ./configure \
	&& make install


WORKDIR "/src"

RUN	tar zxf openwrt.tar.gz

WORKDIR "/src/openwrt-master"

RUN	echo "src-git simetbox https://github.com/simetnicbr/simetbox-openwrt-feed.git" > feeds.conf \
	&& cat feeds.conf.default >> feeds.conf \
	&& cp feeds.conf feeds.conf.default \
	&& make package/symlinks
