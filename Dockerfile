FROM ubuntu:22.04

ARG TARGETARCH

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg
ENV CMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
ENV VCPKG_OVERLAY_TRIPLETS=/opt/vcpkg/custom-triplets

# Base image for building and running the CMake-based C++ server in Docker.
RUN apt-get update -o Acquire::Retries=5 && \
    apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      pkg-config \
      python3 \
      git \
      ca-certificates \
      curl \
      wget \
      zip \
      unzip \
      tar \
      vim \
      tree \
      gdb \
      less \
      man-db \
      manpages \
      manpages-dev \
      linux-libc-dev \
      nlohmann-json3-dev \
      libpq-dev \
      libcurl4-openssl-dev \
      libssl-dev \
      libgtest-dev \
    && yes | unminimize \
    && rm -rf /var/lib/apt/lists/*

COPY vcpkg-triplets ${VCPKG_OVERLAY_TRIPLETS}

RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh \
    && case "${TARGETARCH}" in \
         arm64) VCPKG_TARGET_TRIPLET=arm64-linux-release ;; \
         amd64) VCPKG_TARGET_TRIPLET=x64-linux-release ;; \
         *) echo "Unsupported TARGETARCH=${TARGETARCH}" >&2; exit 1 ;; \
       esac \
    && ${VCPKG_ROOT}/vcpkg install \
         --overlay-triplets=${VCPKG_OVERLAY_TRIPLETS} \
         --triplet=${VCPKG_TARGET_TRIPLET} \
         minio-cpp jwt-cpp

WORKDIR /workspace

COPY . .

RUN case "${TARGETARCH}" in \
      arm64) VCPKG_TARGET_TRIPLET=arm64-linux-release ;; \
      amd64) VCPKG_TARGET_TRIPLET=x64-linux-release ;; \
      *) echo "Unsupported TARGETARCH=${TARGETARCH}" >&2; exit 1 ;; \
    esac \
    && cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DVCPKG_OVERLAY_TRIPLETS=${VCPKG_OVERLAY_TRIPLETS} \
      -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET} \
    && cmake --build build --target webserver --parallel 2

CMD ["./build/bin/webserver", "8080"]
