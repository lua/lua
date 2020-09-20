#https://github.com/KevinDaSilvaS/lua-lang-image
#https://hub.docker.com/r/kevindasilvas/lua

FROM alpine:latest

RUN apk add build-base

RUN apk add curl

RUN apk add --update make

RUN curl -R -O http://www.lua.org/ftp/lua-5.4.0.tar.gz

RUN tar zxf lua-5.4.0.tar.gz

WORKDIR /lua-5.4.0

RUN make all test

WORKDIR /lua-5.4.0/src/

CMD ./lua
