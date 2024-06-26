FROM ubuntu:18.04

ARG n_threads=16
ARG DEBIAN_FRONTEND=noninteractive
ARG TZ=America/New_York
ARG GCC_VER=11

RUN apt-get update && apt-get -y install tzdata 


RUN apt-get install -y locales && rm -rf /var/lib/apt/lists/* \
    && localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8

ENV LANG en_US.utf8

RUN apt-get --allow-releaseinfo-change update && \
    apt-get install -y gtk2.0-dev \
        libfftw3-dev libtiff-dev software-properties-common libffi-dev \
        libbz2-dev libsqlite3-dev zlib1g-dev libjpeg-dev \
        libreadline-dev liblzma-dev libssl-dev libncursesw5-dev wget \
        build-essential \
        && rm -rf /var/lib/apt/lists/*

#Need newer git for Github actions
RUN add-apt-repository ppa:git-core/ppa -y && apt-get --allow-releaseinfo-change update && apt-get install -y git \
    && rm -rf /var/lib/apt/lists/*


RUN wget -q https://github.com/wxWidgets/wxWidgets/releases/download/v3.0.5/wxWidgets-3.0.5.tar.bz2 -O /tmp/wxwidgets.tar.bz2 && \
    echo 'Installing wxWidgets' && \
    tar -xf /tmp/wxwidgets.tar.bz2 -C /tmp && \
    cd /tmp/wxWidgets-3.0.5  && \
    CXXFLAGS=-fPIC CFLAGS=-fPIC ./configure --disable-precomp-headers --prefix=/usr/local --with-libnotify=no --disable-shared --without-gtkprint --with-libjpeg=builtin --with-libpng=builtin --with-libtiff=builtin --with-zlib=builtin --with-expat=builtin --disable-compat28 --without-liblzma --without-libjbig --with-gtk=2 --disable-sys-libs && \
    make -j$n_threads && \
    make install
