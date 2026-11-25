FROM docker.io/library/debian:trixie-slim AS llvm-runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && curl --fail --silent --show-error --location https://apt.llvm.org/llvm-snapshot.gpg.key \
        -o /usr/share/keyrings/llvm-snapshot.asc \
    && printf '%s\n' 'deb [signed-by=/usr/share/keyrings/llvm-snapshot.asc] https://apt.llvm.org/trixie/ llvm-toolchain-trixie-22 main' \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends clang-22 llvm-22 libz3-4 \
    && rm -rf /var/lib/apt/lists/*

ENV PATH="/usr/lib/llvm-22/bin:${PATH}"

FROM llvm-runtime AS toolchain

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake g++ git libz3-dev llvm-22-dev ninja-build zlib1g-dev libzstd-dev libxml2-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang CXX=clang++

FROM toolchain AS svf

ARG SVF_COMMIT=a8a0f5181748059c2df2281e5e4ada63ccda3832
ARG BUILD_JOBS=4
RUN git init /src/svf \
    && git -C /src/svf remote add origin https://github.com/SVF-tools/SVF.git \
    && git -C /src/svf fetch --depth=1 origin "${SVF_COMMIT}" \
    && git -C /src/svf checkout --detach FETCH_HEAD \
    && cmake -S /src/svf -B /build/svf -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DSVF_WARN_AS_ERROR=OFF -DSVF_DEBUG_INFO=OFF \
        -DCMAKE_INSTALL_PREFIX=/opt/svf -DLLVM_DIR="$(llvm-config --cmakedir)" \
    && cmake --build /build/svf --parallel "${BUILD_JOBS}" \
    && cmake --install /build/svf

FROM svf AS build

ARG VERIFIER_GIT_COMMIT
WORKDIR /src/verifier
COPY CMakeLists.txt ./
COPY include/ include/
COPY src/ src/
COPY tests/ tests/
RUN cmake -S . -B /build/verifier -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DSVF_DIR=/opt/svf/lib/cmake/SVF -DLLVM_DIR="$(llvm-config --cmakedir)" \
        "-DSVF_COMMIT=${SVF_COMMIT}" "-DVERIFIER_GIT_COMMIT=${VERIFIER_GIT_COMMIT}" \
    && cmake --build /build/verifier --parallel "${BUILD_JOBS}" \
    && ctest --test-dir /build/verifier --output-on-failure

FROM llvm-runtime AS runtime

COPY --from=build /build/verifier/svf-verifier /usr/local/bin/
COPY --from=svf /opt/svf/lib/extapi.bc /opt/svf/lib/
COPY include/verifier.h /usr/local/include/
WORKDIR /work
CMD ["svf-verifier", "--help"]
