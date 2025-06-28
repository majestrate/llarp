FROM debian:stable

WORKDIR /usr/local/src
COPY . ./llarp
RUN apt update -q -y && apt upgrade -q -y
RUN apt install -q -y build-essential cmake git pkg-config automake libtool libuv1-dev libsodium-dev libsystemd-dev nettle-dev libevent-dev libunbound-dev libjemalloc-dev ninja-build
RUN mkdir -p llarp/build && cd llarp && cmake -B build -S . -DNATIVE_BUILD=OFF -DWITH_SETCAP=OFF -G Ninja && ninja -C build && ninja -C build install
RUN mkdir -p /var/lib/llarpd/
RUN mkdir -p /data/
COPY ./contrib/docker/entrypoint.sh /entrypoint.sh
COPY ./contrib/docker/llarpd.ini /var/lib/llarpd/llarpd.ini
COPY ./contrib/bootstrap/opennet.signed /var/lib/llarpd/bootstrap.signed
RUN rm -rf /usr/local/src/llarp
ENTRYPOINT /entrypoint.sh
