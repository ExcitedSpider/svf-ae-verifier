ARG LLVM_MAJOR=22
ARG SVF_REF=master
ARG BUILD_JOBS=4
ARG VERIFIER_GIT_COMMIT=unknown

FROM docker.io/library/debian:trixie-slim AS llvm-runtime

ARG LLVM_MAJOR

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && curl --fail --silent --show-error --location https://apt.llvm.org/llvm-snapshot.gpg.key \
        -o /usr/share/keyrings/llvm-snapshot.asc \
    && printf '%s\n' "deb [signed-by=/usr/share/keyrings/llvm-snapshot.asc] https://apt.llvm.org/trixie/ llvm-toolchain-trixie-${LLVM_MAJOR} main" \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends "clang-${LLVM_MAJOR}" "llvm-${LLVM_MAJOR}" libz3-4 \
    && rm -rf /var/lib/apt/lists/*

ENV PATH="/usr/lib/llvm-${LLVM_MAJOR}/bin:${PATH}"

FROM llvm-runtime AS toolchain

ARG LLVM_MAJOR

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake g++ git libz3-dev "llvm-${LLVM_MAJOR}-dev" ninja-build zlib1g-dev libzstd-dev libxml2-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang CXX=clang++

FROM toolchain AS svf

ARG SVF_REF
ARG BUILD_JOBS
RUN git init /src/svf \
    && git -C /src/svf remote add origin https://github.com/SVF-tools/SVF.git \
    && git -C /src/svf fetch --depth=1 origin "${SVF_REF}" \
    && git -C /src/svf checkout --detach FETCH_HEAD \
    && cmake -S /src/svf -B /build/svf -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DSVF_WARN_AS_ERROR=OFF -DSVF_DEBUG_INFO=OFF \
        -DCMAKE_INSTALL_PREFIX=/opt/svf -DLLVM_DIR="$(llvm-config --cmakedir)" \
    && cmake --build /build/svf --parallel "${BUILD_JOBS}" \
    && cmake --install /build/svf \
    && git -C /src/svf rev-parse HEAD > /opt/svf/SVF_COMMIT \
    && llvm-config --version > /opt/svf/LLVM_VERSION

FROM svf AS build

# Python's jsonschema validates every test report against schema/report.schema.json.
RUN apt-get update \
    && apt-get install -y --no-install-recommends python3-jsonschema \
    && rm -rf /var/lib/apt/lists/*

ARG VERIFIER_GIT_COMMIT
ARG VERIFIER_VERSION
ARG BUILD_JOBS
WORKDIR /src/verifier
COPY CMakeLists.txt ./
COPY include/ include/
COPY src/ src/
COPY tests/ tests/
COPY schema/ schema/
COPY docs/report.md docs/
RUN cmake -S . -B /build/verifier -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DSVF_DIR=/opt/svf/lib/cmake/SVF -DLLVM_DIR="$(llvm-config --cmakedir)" \
        "-DVERIFIER_GIT_COMMIT=${VERIFIER_GIT_COMMIT}" \
        "-DVERIFIER_RELEASE_VERSION=${VERIFIER_VERSION}" \
        -DVERIFIER_REQUIRE_SCHEMA_CHECK=ON \
    && cmake --build /build/verifier --parallel "${BUILD_JOBS}" \
    && ctest --test-dir /build/verifier --output-on-failure \
    && printf '%s\n' "${VERIFIER_GIT_COMMIT:-unknown}" > /opt/svf/VERIFIER_COMMIT

FROM llvm-runtime AS runtime

COPY --from=build /build/verifier/svf-verifier /usr/local/bin/
COPY --from=svf /opt/svf/lib/extapi.bc /opt/svf/lib/
COPY --from=svf /opt/svf/SVF_COMMIT /opt/svf/LLVM_VERSION /opt/svf/
COPY --from=build /opt/svf/VERIFIER_COMMIT /opt/svf/
COPY include/verifier.h /usr/local/include/
COPY schema/report.schema.json /usr/local/share/svf-ae-verifier/
WORKDIR /work
CMD ["svf-verifier", "--help"]
