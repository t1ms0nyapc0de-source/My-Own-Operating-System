FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
ENV PREFIX="/root/opt/cross"
ENV TARGET="i686-elf"
ENV PATH="/root/opt/cross/bin:${PATH}"

RUN apt update && apt install -y \
    build-essential bison flex \
    libgmp3-dev libmpc-dev libmpfr-dev \
    texinfo nasm xorriso mtools \
    grub-pc-bin grub-common curl \
    qemu-system-x86

RUN mkdir -p /root/src && cd /root/src && \
    curl -O https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.gz && \
    curl -O https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz && \
    tar xf binutils-2.41.tar.gz && \
    tar xf gcc-13.2.0.tar.gz

RUN cd /root/src && mkdir build-binutils && cd build-binutils && \
    ../binutils-2.41/configure --target=$TARGET --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror && \
    make && make install

RUN cd /root/src && mkdir build-gcc && cd build-gcc && \
    ../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" \
    --disable-nls --enable-languages=c,c++ --without-headers && \
    make all-gcc && make all-target-libgcc && \
    make install-gcc && make install-target-libgcc

WORKDIR /root/kernel