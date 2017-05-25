# Base image
FROM ubuntu:latest
MAINTAINER "Fabio Rodrigues Ribeiro <farribeiro@gmail.com>"

ADD https://github.com/simetnicbr/simetbox-openwrt-base/archive/master.zip /src

WORKDIR "/src/simetbox-openwrt-base-master"

