FROM alpine:edge
MAINTAINER John Torres <enfermo337@yahoo.com>

WORKDIR /workspace

RUN apk add --no-cache libgcc libstdc++ libcrypto1.0 libssl1.0 openssl-dev gcc g++ make git && \
    git clone https://github.com/AGWA/git-crypt /workspace && \
    make && \
    make install PREFIX=/usr/local && \
    apk del openssl-dev gcc g++ make git && \
    rm -rf /workspace
