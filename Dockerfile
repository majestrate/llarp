FROM debian:stable-slim AS build

RUN apt update -q -y && apt upgrade -q -y
RUN apt install -q -y libuv1-dev libsodium-dev nettle-dev libevent-dev libunbound-dev libjemalloc-dev
RUN apt install -q -y build-essential cmake git pkg-config automake libtool ninja-build
WORKDIR /usr/local/src
COPY . ./llarp
RUN mkdir -p llarp/build && cd llarp && cmake -B build -S . -DNATIVE_BUILD=OFF -DWITH_SETCAP=OFF -G Ninja && ninja -C build && ninja -C build install


FROM debian:stable-slim

RUN apt update -q -y && apt upgrade -q -y
RUN apt install -q -y libuv1-dev libsodium-dev nettle-dev libevent-dev libunbound-dev libjemalloc-dev
RUN mkdir -p /var/lib/llarpd/
RUN mkdir -p /data/
COPY ./contrib/docker/entrypoint.sh /entrypoint.sh
COPY ./contrib/docker/llarpd.ini /var/lib/llarpd/llarpd.ini
COPY ./contrib/bootstrap/opennet.signed /var/lib/llarpd/bootstrap.signed
COPY --from=build /usr/local/bin/llarpd /usr/local/bin/
ENTRYPOINT /entrypoint.sh
