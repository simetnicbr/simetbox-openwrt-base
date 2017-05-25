# Base image
FROM ubuntu:latest
MAINTAINER "Fabio Rodrigues Ribeiro <farribeiro@gmail.com>"

ADD https://github.com/simetnicbr/simetbox-openwrt-base/archive/master.zip /src

WORKDIR "/src/simetbox-openwrt-base-master"

RUN	apt-get update \
	&& apt-get upgrade -y \
	&& apt-get install -y \
	build-essential \
	--no-install-recommends \
	&& autoreconf \
	&& automake --add-missing \
	&& ./configure \
	&& make install
