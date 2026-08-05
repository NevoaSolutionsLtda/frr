# syntax=docker/dockerfile:1.6
# Builder/runtime for testing mgmtd gRPC work (PR #22158 and related).
# Build context: a checkout of the tree under test.
ARG DEBIAN=bookworm-slim
ARG FRR_SRC_DIR=/src/frr

FROM debian:${DEBIAN} AS builder
ARG FRR_SRC_DIR
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      git autoconf automake libtool make pkg-config build-essential cmake \
      bison flex texinfo install-info ca-certificates python3-dev \
      python3-sphinx libreadline-dev libjson-c-dev libc-ares-dev \
      protobuf-c-compiler libprotobuf-c-dev libgrpc++-dev libprotobuf-dev \
      protobuf-compiler-grpc libsnmp-dev libcap-dev libelf-dev libpcre2-dev \
    && rm -rf /var/lib/apt/lists/*
ARG LIBYANG_VERSION=v2.1.148
RUN git clone --depth 1 --branch ${LIBYANG_VERSION} https://github.com/CESNET/libyang.git /src/libyang \
 && cmake -S /src/libyang -B /src/libyang/build -DCMAKE_INSTALL_PREFIX=/usr \
      -DCMAKE_BUILD_TYPE=Release -DENABLE_LYD_PRIV=ON \
 && cmake --build /src/libyang/build -j"$(nproc)" \
 && DESTDIR=/libyang-dist cmake --install /src/libyang/build \
 && cmake --install /src/libyang/build && ldconfig
WORKDIR ${FRR_SRC_DIR}
COPY . ${FRR_SRC_DIR}
RUN ./bootstrap.sh \
 && ./configure --prefix=/usr/local --sysconfdir=/etc/frr \
      --localstatedir=/var/run/frr --sbindir=/usr/local/sbin \
      --enable-grpc --enable-mgmtd --enable-mgmtd-test-be-client \
      --enable-user=frr --enable-group=frr --enable-vty-group=frrvty \
 && make -j"$(nproc)" && make install DESTDIR=/out

FROM debian:${DEBIAN}
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      libpcre2-8-0 libprotobuf-c1 libgrpc++1.51 libjson-c5 libc-ares2 \
      libreadline8 libcap2-bin iproute2 tini ca-certificates \
      python3 python3-pip python3-pytest python3-grpc-tools gdb \
    && rm -rf /var/lib/apt/lists/*
RUN groupadd -r frr && groupadd -r frrvty \
 && useradd -r -g frr -G frrvty -s /sbin/nologin frr \
 && mkdir -p /etc/frr /var/log/frr /var/run/frr \
 && chown -R frr:frr /etc/frr /var/log/frr /var/run/frr
COPY --from=builder /libyang-dist/usr/ /usr/
RUN ldconfig
COPY --from=builder /out/ /
RUN echo '/usr/local/lib' > /etc/ld.so.conf.d/frr.conf && ldconfig \
 && mkdir -p /usr/lib && ln -sfn /usr/local/lib/frr /usr/lib/frr \
 && for b in /usr/local/sbin/*; do ln -sf "$b" "/usr/lib/frr/$(basename "$b")"; done
ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["/bin/bash"]
