# SVF AE verifier

`svf-verifier` checks reachability assertions and load/store bounds using the final states computed by SVF abstract execution. It emits one JSON report per run with a verdict for each site and each selected property. `proved` means the check holds in the available AE model. `unproved` means the tool did not establish the check.

## Build

Use LLVM 22, CMake, Ninja, Z3 development files and the same Clang C++ compiler for SVF and the verifier. Matching compilers keeps SVF's option registry shared across both. The supported SVF revision is `a8a0f5181748059c2df2281e5e4ada63ccda3832` (SVF 3.4). Run these commands from the verifier checkout in a development environment with LLVM 22 on `PATH`.

```sh
git clone https://github.com/SVF-tools/SVF.git ../svf-verifier-deps/svf
git -C ../svf-verifier-deps/svf checkout --detach a8a0f5181748059c2df2281e5e4ada63ccda3832
cmake -S ../svf-verifier-deps/svf -B ../svf-verifier-deps/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DSVF_WARN_AS_ERROR=OFF -DCMAKE_INSTALL_PREFIX="$PWD/../svf-verifier-deps/install" \
  -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build ../svf-verifier-deps/build
cmake --install ../svf-verifier-deps/build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DSVF_DIR="$PWD/../svf-verifier-deps/install/lib/cmake/SVF" \
  -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build build
```

SVF's default warnings-as-errors setting rejects unused variables in the pinned revision's Release build. The commands disable that setting using SVF's CMake option and keep the source unmodified.

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

[Property language and verdicts](docs/properties.md) · [JSON report schema and example](docs/report.md)
