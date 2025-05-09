FROM ubuntu:20.04

# /groups/himesb/.config/Code\ -\ Insiders/User/globalStorage/ms-vscode-remote.remote-containers/cli-bin/code-insiders build --image-name bhimesbhimes/cistem_build_env:latest ./.devcontainer_build

# For compiling wxWidgets
ARG n_threads=16
ARG DEBIAN_FRONTEND=noninteractive
ARG TZ=America/New_York
ARG GCC_VER=11

# By default this will create cisTEMdev as uid==1000 which is not what we want as it is also going to be the user id
# of a host user when building the top layer with singularity. < 1000 is reserved for system users. 814 is a somewhat random choice.
RUN apt-get update && \
    apt-get -y install tzdata sudo && \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone && \
    apt-get install -y locales && \
    localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8 && \
    useradd --uid 814 -ms /bin/bash cisTEMdev && \
    echo "cisTEMdev:cisTEMdev" | chpasswd && adduser cisTEMdev sudo && \
    rm -rf /var/lib/apt/lists/*

ENV LANG en_US.utf8

# Install useful ubuntu packages libwxgtk3.0-gtk3-dev libwxbase3.0-dev
RUN apt-get --allow-releaseinfo-change update && apt-get install -y \
    libgtk2.0-dev libgtk-3-dev libwxbase3.0-dev \
    libtool autoconf autotools-dev nano gedit meld cmake \
    libfftw3-dev libtiff-dev software-properties-common libffi-dev \
    libbz2-dev libsqlite3-dev zlib1g-dev libjpeg-dev libtiff-dev \
    libreadline-dev liblzma-dev libssl-dev libncursesw5-dev wget \
    build-essential git xauth zip unzip parallel sqlite3 python3 python3-pip curl gdb &&\
    rm -rf /var/lib/apt/lists/*

# Install clang format 14 - these goa t the end of /etc/apt/sources.list
RUN echo "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-14 main" | tee -a /etc/apt/sources.list && \
    echo "deb-src http://apt.llvm.org/focal/ llvm-toolchain-focal-14 main" | tee -a /etc/apt/sources.list && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add - && \
    apt-get update && apt-get install -y clang-format-14 clang-14 clang-tidy-14 && \
    cd /usr/bin && ln -s clang-format-14 clang-format && ln -s clang-14 clang && ln -s clang-tidy-14 clang-tidy && ln -s clang++-14 clang++ \
    && rm -rf /var/lib/apt/lists/*



# Get the MKL and intel compiler: note, this is 19G by default, will try to determine minimal set needed huge waste but works well enough for now, final size is ~5gb
# The second to last line is to ensure clang++ is used and not the one bundled with the intel compiler.
RUN wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | gpg --dearmor | tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null && \
    echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | tee /etc/apt/sources.list.d/oneAPI.list && \
    apt-get update && apt-get install -y intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic-2021.4.0 intel-oneapi-mkl-2021.4.0 intel-oneapi-mkl-devel-2021.4.0 && \
    cd /opt/intel/oneapi/ && rm -rf debugger/ conda_channel/ mkl/latest/lib/intel64/*_sycl* compiler/2021.4.0/linux/lib/oclfpga && \
    mkdir -p /opt/intel && echo 'int mkl_serv_intel_cpu_true() {return 1;}' > /opt/intel/fakeIntel.c && \
    gcc -shared -fPIC -o /opt/intel/libfakeIntel.so /opt/intel/fakeIntel.c && \
    rm -rf /var/lib/apt/lists/* 

# Pre-empt the intel function that checks intel or not at runtime so that optimal codepaths may be used on AMD procs.
# This is only needed for dynamic builds. Note the lib is build in the next step.
ENV LD_PRELOAD=/opt/intel/libfakeIntel.so

# Grab the source for wxWidgets 3.0.5, the stable version and build it against gtk-2 for both dyamic and staic linking
# Installation is deferred until the final layer so only the correct version is installed to prevent difficult to track down conflicts.
# Neither will be used for the dev version 3.1.5 or 3.2 which will be linked against gtk-3
# NOTE: for now, only building with the intel-compiler. If people still are pushing gnu compiler, we can add those here.
RUN wget -q https://github.com/wxWidgets/wxWidgets/releases/download/v3.0.5/wxWidgets-3.0.5.tar.bz2 -O /tmp/wxwidgets.tar.bz2 && \
    echo 'Building wxWidgets but deferring install' && \
    mkdir -p /opt/WX/icc-static /opt/WX/gcc-static /opt/WX/clang-static && \
    tar -xjf /tmp/wxwidgets.tar.bz2 -C /opt/WX/icc-static && \
    tar -xjf /tmp/wxwidgets.tar.bz2 -C /opt/WX/gcc-static && \
    tar -xjf /tmp/wxwidgets.tar.bz2 -C /opt/WX/clang-static && \
    rm -rf /tmp/wxwidgets.tar.bz2 

# Here for the record if you want to build and link static binaries to avoid the cointainerized distribution
# CXXFLAGS=-fPIC CFLAGS=-fPIC were specified for the static configure line, but I'm not sure that makes any sense?
RUN . /opt/intel/oneapi/setvars.sh && \
    cd /opt/WX && wget https://github.com/wxFormBuilder/wxFormBuilder/releases/download/v3.10.0/wxformbuilder_3.10.0_ubuntu-20.04_amd64.deb && \
    cd /opt/WX/icc-static/wxWidgets-3.0.5 && \
    CXXFLAGS=-fPIC CFLAGS=-fPIC CXX=icpc CC=icc ./configure --disable-precomp-headers --prefix=/opt/WX/icc-static --with-libnotify=no --disable-shared \
    --without-gtkprint --with-libjpeg=builtin --with-libpng=builtin --with-libtiff=builtin --with-zlib=builtin --with-expat=builtin \
    --disable-compat28 --without-liblzma --without-libjbig --with-gtk=2 --disable-sys-libs  && \
    make -j$n_threads && make install && make clean && make distclean && rm -rf /opt/WX/icc-static/wxWidgets-3.0.5 && \
    cd /opt/WX/gcc-static/wxWidgets-3.0.5 && \
    CXXFLAGS=-fPIC CFLAGS=-fPIC CXX=g++ CC=gcc ./configure --disable-precomp-headers --prefix=/opt/WX/gcc-static --with-libnotify=no --disable-shared \
    --without-gtkprint --with-libjpeg=builtin --with-libpng=builtin --with-libtiff=builtin --with-zlib=builtin --with-expat=builtin \
    --disable-compat28 --without-liblzma --without-libjbig --with-gtk=2 --disable-sys-libs  && \
    make -j$n_threads && make install && make clean && make distclean && rm -rf /opt/WX/gcc-static/wxWidgets-3.0.5 && \
    cd /opt/WX/clang-static/wxWidgets-3.0.5 && \
    CXXFLAGS=-fPIC CFLAGS=-fPIC CXX=clang++ CC=clang ./configure --disable-precomp-headers --prefix=/opt/WX/clang-static --with-libnotify=no --disable-shared \
    --without-gtkprint --with-libjpeg=builtin --with-libpng=builtin --with-libtiff=builtin --with-zlib=builtin --with-expat=builtin \
    --disable-compat28 --without-liblzma --without-libjbig --with-gtk=2 --disable-sys-libs  && \
    make -j$n_threads && make install && make clean && make distclean && rm -rf /opt/WX/clang-static/wxWidgets-3.0.5 && \
    tf=`tempfile` && cp /opt/WX/icc-static/include/wx-3.0/wx/longlong.h /opt/WX/icc-static/include/wx-3.0/wx/longlong.h.orig && \
    awk '{if(/#include "wx\/defs.h"/){ print $0 ;print "#include <wx/txtstrm.h>"} else print $0}' /opt/WX/icc-static/include/wx-3.0/wx/longlong.h.orig > $tf && \
    mv $tf /opt/WX/icc-static/include/wx-3.0/wx/longlong.h && \
    chmod a+r /opt/WX/icc-static/include/wx-3.0/wx/longlong.h && \
    tf=`tempfile` && cp /opt/WX/gcc-static/include/wx-3.0/wx/longlong.h /opt/WX/gcc-static/include/wx-3.0/wx/longlong.h.orig && \
    awk '{if(/#include "wx\/defs.h"/){ print $0 ;print "#include <wx/txtstrm.h>"} else print $0}' /opt/WX/gcc-static/include/wx-3.0/wx/longlong.h.orig > $tf && \
    mv $tf /opt/WX/gcc-static/include/wx-3.0/wx/longlong.h && \
    chmod a+r /opt/WX/gcc-static/include/wx-3.0/wx/longlong.h && \
    tf=`tempfile` && cp /opt/WX/clang-static/include/wx-3.0/wx/longlong.h /opt/WX/clang-static/include/wx-3.0/wx/longlong.h.orig && \
    awk '{if(/#include "wx\/defs.h"/){ print $0 ;print "#include <wx/txtstrm.h>"} else print $0}' /opt/WX/clang-static/include/wx-3.0/wx/longlong.h.orig > $tf && \
    mv $tf /opt/WX/clang-static/include/wx-3.0/wx/longlong.h && \
    chmod a+r /opt/WX/clang-static/include/wx-3.0/wx/longlong.h



# First noticed outside container with g++9, several errors in longlong.h seem to be fixed by this extra include  /usr/include/wx-3.1-unofficial


# Install pytorch - now optional in the top layer container


# Install cuda (when the web is live)
# NOTE: the MINIMUM driver verions is >=525.60.13
ARG CUDA_VER=12.3.2
ARG DRIVER_VER=545.23.08

# Install cuda (when the web is live)
RUN cd /tmp && wget https://developer.download.nvidia.com/compute/cuda/${CUDA_VER}/local_installers/cuda_${CUDA_VER}_${DRIVER_VER}_linux.run && \
    sh cuda_${CUDA_VER}_${DRIVER_VER}_linux.run --silent --toolkit && \
    rm cuda_${CUDA_VER}_${DRIVER_VER}_linux.run && \
    cd /usr/local/cuda/lib64 && rm -rf libcuspars* libcusolver* libcublasLt* libnppif* nsight-compute-* nsight-systems-* nsightee_plugins

# Note that for dynamic builds, the base container will be larger than the top layer because we defer removing the static cuda libs until the end

RUN echo 'alias lt="ls -lrth"' >> /home/cisTEMdev/.bashrc && \
    echo 'alias dU="du -ch --max-depth=1 | sort -h"' >> /home/cisTEMdev/.bashrc && \
    echo 'source /opt/intel/oneapi/setvars.sh' >> /home/cisTEMdev/.bashrc && \
    echo 'bind "set bell-style none"' >> /home/cisTEMdev/.bashrc && \
    echo 'export PATH=/usr/bin:/usr/local/cuda/bin:$PATH' >> /home/cisTEMdev/.bashrc 






