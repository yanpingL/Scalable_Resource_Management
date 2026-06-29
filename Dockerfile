# syntax=docker/dockerfile:1.7

FROM ubuntu:22.04 AS build

ARG TARGETARCH

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg
ENV CMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
ENV VCPKG_OVERLAY_TRIPLETS=/opt/vcpkg-triplets

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update -o Acquire::Retries=5 && \
    apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      cmake \
      curl \
      git \
      libcurl4-openssl-dev \
      libgtest-dev \
      libpq-dev \
      libssl-dev \
      ninja-build \
      nlohmann-json3-dev \
      pkg-config \
      python3 \
      tar \
      unzip \
      zip

COPY vcpkg-triplets ${VCPKG_OVERLAY_TRIPLETS}

RUN --mount=type=cache,target=/var/cache/vcpkg,sharing=locked \
    --mount=type=cache,target=/root/.cache/vcpkg,sharing=locked \
    git clone --depth 1 https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} && \
    ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics && \
    case "${TARGETARCH}" in \
      arm64) VCPKG_TARGET_TRIPLET=arm64-linux-release ;; \
      amd64) VCPKG_TARGET_TRIPLET=x64-linux-release ;; \
      *) echo "Unsupported TARGETARCH=${TARGETARCH}" >&2; exit 1 ;; \
    esac && \
    VCPKG_BINARY_SOURCES="clear;files,/var/cache/vcpkg,readwrite" \
      ${VCPKG_ROOT}/vcpkg install \
        --overlay-triplets=${VCPKG_OVERLAY_TRIPLETS} \
        --triplet=${VCPKG_TARGET_TRIPLET} \
        "aws-sdk-cpp[s3]" minio-cpp jwt-cpp "hiredis[ssl]" && \
    cp -a ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET} /opt/vcpkg-runtime

WORKDIR /workspace

COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests

RUN case "${TARGETARCH}" in \
      arm64) VCPKG_TARGET_TRIPLET=arm64-linux-release ;; \
      amd64) VCPKG_TARGET_TRIPLET=x64-linux-release ;; \
      *) echo "Unsupported TARGETARCH=${TARGETARCH}" >&2; exit 1 ;; \
    esac && \
    cmake -S . -B build \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET} && \
    cmake --build build --target webserver unit_tests --parallel 2

FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV LD_LIBRARY_PATH=/opt/vcpkg-runtime/lib

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update -o Acquire::Retries=5 && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      libcurl4 \
      libpq5 \
      libssl3 \
      postgresql-client

WORKDIR /workspace

COPY --from=build /opt/vcpkg-runtime /opt/vcpkg-runtime
COPY --from=build /workspace/build/bin ./build/bin
COPY db/migrations ./db/migrations
COPY scripts ./scripts

CMD ["./build/bin/webserver", "8080"]
