# Base image
FROM debian:jessie
MAINTAINER "Fabio Rodrigues Ribeiro <farribeiro@gmail.com>"

ADD https://github.com/simetnicbr/simetbox-openwrt-base/archive/master.tar.gz /src/simetbox-openwrt-base.tar.gz
ADD https://github.com/simetnicbr/simetbox-openwrt-config/archive/master.tar.gz /src/simetbox-openwrt-config.tar.gz
ADD https://github.com/openwrt/openwrt/archive/chaos_calmer.tar.gz /src/openwrt.tar.gz

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
	python \
	wget \
	rsync \
	--no-install-recommends \
	&& mkdir -p /src/simetbox \
	&& tar zxf simetbox-openwrt-base.tar.gz \
	&& tar zxf simetbox-openwrt-config.tar.gz \
	&& tar zxf openwrt.tar.gz \
	&& rsync -av /src/simetbox-openwrt-base-master/* /src/simetbox \
	&& rsync -av /src/simetbox-openwrt-config-master/* /src/simetbox

WORKDIR "/src/simetbox"

RUN	autoreconf ; exit 0

RUN	automake --add-missing \
	&& ./configure \
	&& make install

WORKDIR "/src/openwrt-chaos_calmer"

RUN	echo "src-git simetbox https://github.com/simetnicbr/simetbox-openwrt-feed.git" > feeds.conf \
	&& cat feeds.conf.default >> feeds.conf \
	&& cp feeds.conf feeds.conf.default \
	&& make package/symlinks

COPY	.config /src/openwrt-chaos_calmer/.config
