FROM alpine:edge
MAINTAINER John Torres <enfermo337@yahoo.com>

ARG VERSION
ARG GIT_COMMIT
ARG GIT_DIRTY
COPY . /workspace/

WORKDIR /workspace

RUN echo "BUILD: $VERSION (${GIT_COMMIT}${GIT_DIRTY})" > /IMAGE_VERSION && \
    apk add --no-cache libgcc libstdc++ libcrypto1.0 libssl1.0 openssl-dev gcc g++ make git && \
    make && \
    make install PREFIX=/usr/local && \
    apk del --no-cache openssl-dev gcc g++ make git && \
    rm -rf /workspace

ENTRYPOINT ["/usr/local/bin/git-crypt"]
CMD []
