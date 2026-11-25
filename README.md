# SVF AE verifier

`svf-verifier` checks reachability assertions and load/store bounds using the final states computed by SVF abstract execution. It emits one JSON report per run with a verdict for each site and each selected property. `proved` means the check holds in the available AE model. `unproved` means the tool did not establish the check.

SVF-AE computes the abstract-state fixpoint, and the fixpoint must be sound. The verifier takes that state, checks properties against it, and produces the alarms, their locations and the report. See [Responsibilities](DESIGN.md#responsibilities).

## Build and run in a container

The Dockerfile is the reference build. It builds SVF `master` and the verifier with the same LLVM 22 compiler and runs CTest during the build.

```sh
docker build --no-cache -t svf-ae-verifier \
  --build-arg "VERIFIER_GIT_COMMIT=$(git rev-parse HEAD)" .
docker run --rm --memory=4g --memory-swap=4g -v "$PWD:/input:ro" svf-ae-verifier \
  sh -ec 'clang -g -S -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names \
    /input/tests/reach/conditions.c -o input.raw.ll
  opt -S -p=mem2reg input.raw.ll -o input.ll
  timeout 120 svf-verifier -property=reach -ae-sparsity=sparse input.ll'
```

The same commands work with `podman` in place of `docker`. On SELinux hosts use `:ro,Z` for the bind mount if needed.

Run the build from a clean checkout to record its commit. Omit `VERIFIER_GIT_COMMIT` if the verifier revision is unknown. Pass `--build-arg SVF_REF=<commit>` to select a specific SVF revision or `--build-arg LLVM_MAJOR=<major>` to select a different LLVM toolchain. The report records `svf_commit` and `llvm_version`.

## Manual build against installed SVF

Tested with SVF `master`. Use LLVM 22, CMake, Ninja, Z3 development files and the same Clang C++ compiler for SVF and the verifier. Matching compilers keeps SVF's option registry shared across both. Run these commands from the verifier checkout in a development environment with LLVM 22 on `PATH`.

```sh
git clone --branch master https://github.com/SVF-tools/SVF.git ../svf-verifier-deps/svf
cmake -S ../svf-verifier-deps/svf -B ../svf-verifier-deps/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DSVF_WARN_AS_ERROR=OFF -DCMAKE_INSTALL_PREFIX="$PWD/../svf-verifier-deps/install" \
  -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build ../svf-verifier-deps/build
cmake --install ../svf-verifier-deps/build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DSVF_DIR="$PWD/../svf-verifier-deps/install/lib/cmake/SVF" \
  "-DSVF_COMMIT=$(git -C ../svf-verifier-deps/svf rev-parse HEAD)" \
  -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build build
```

SVF's default warnings-as-errors setting can reject unused variables in Release builds. The commands disable that setting without modifying SVF. For an existing SVF installation, supply `-DSVF_COMMIT=<commit>` only when you know which revision built it. If the installation has no revision metadata, the report uses `unknown`.

## Compile and verify a C file

Include `verifier.h` in your source to use the `reach` macros. The repository's `tests/reach/conditions.c` is a standalone example.

```sh
clang -g -S -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names \
  -Iinclude tests/reach/conditions.c -o example.raw.ll
opt -S -p=mem2reg example.raw.ll -o example.ll
(ulimit -v 4194304 && timeout 120 build/svf-verifier -property=reach \
  -ae-sparsity=sparse example.ll -o report.json)
```

Select `reach`, `overflow` or both with `-property=reach,overflow`. Pass `-ae-sparsity=dense`, `semi-sparse` or `sparse`. Dense mode is the SVF default. Omit `-o` to write the report to stdout. Use `--help` for SVF's option list.

Exit `0` means analysis and report writing finished, including runs with unproved sites. Exit `1` signals an analysis, input or report failure. Exit `2` signals bad usage.

## Run the tests

```sh
ctest --test-dir build --output-on-failure
```

CTest compiles fixtures and checks all three modes. On Linux the test harness requires Bash and `timeout` to apply the 120-second and 4 GiB virtual-memory limits.

[Property language and verdicts](docs/properties.md) · [JSON report format and example](docs/report.md) · [JSON Schema](schema/report.schema.json)
