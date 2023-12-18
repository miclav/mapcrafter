#
# Build Image
#

FROM alpine:3.8 as builder

# Add the git repo
ADD . /git/mapcrafter

# Dependencies needed for building Mapcrafter
# (not sure how many of these are actually needed)
RUN apk add --no-cache --upgrade \
    cmake \
    gcc \
    make \
    g++ \
    zlib-dev \
    libpng-dev \
    libjpeg-turbo-dev \
    boost-dev

# Build mapcrafter from source
RUN cd /git/mapcrafter && \
    mkdir -p build && cd build && \
    cmake .. -DOPT_OPTIMIZE=ON -DOPT_SKIP_TESTS=ON && \
    make && \
    mkdir /tmp/mapcrafter && \
    make DESTDIR=/tmp/mapcrafter install


#
# Final Image
#

FROM alpine:3.8

# Mapcrafter, built in previous stage
COPY --from=builder /tmp/mapcrafter/ /
COPY --from=miclav/mapcrafter-blocks /blocks /usr/local/share/mapcrafter/blocks

# Depedencies needed for running Mapcrafter
RUN apk add --no-cache --upgrade \
    libpng \
    libjpeg-turbo \
    boost \
    boost-iostreams \
    boost-system \
    boost-filesystem \
    boost-program_options \
    shadow

# Entrypoint
ADD entrypoint*.sh /
ENTRYPOINT ["/entrypoint.sh"]
