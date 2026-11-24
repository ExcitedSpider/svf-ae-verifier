# SVF AE verifier

This proof of concept reads SVF Abstract Execution's final states and emits one JSON record for each reachability check. It supports dense, semi-sparse and sparse AE.

| Path | Responsibility |
| --- | --- |
| `include/Verifier/` and `lib/Verifier/` | LLVM-free site discovery, final-state queries and verdicts |
| `tools/verifier/` | Module loading, AE setup and JSON output |
| `runner/` | Compilation, process limits and evaluation reports |

Use an unmodified SVF checkout at `a8a0f5181748059c2df2281e5e4ada63ccda3832` in `svf/` and the `localhost/svf22-z3:llvm22` Podman image. The build below mounts SVF read-only and puts all build output in `build/`. Set `kit` to the directory containing the reachability kit.

```sh
kit=/path/to/kit
mkdir -p build
podman run --rm --pull=never --network=none \
  -v "$PWD:/src:ro" -v "$PWD/svf:/svf:ro" \
  -v "$PWD/build:/work/build:rw" -v "$kit:/e:ro" \
  -w /src localhost/svf22-z3:llvm22 sh -ec '
    PYTHONDONTWRITEBYTECODE=1 python3 -c "from runner.runner import build; build()"
    cd "$(mktemp -d)"
    export PATH="/opt/llvm/bin:$PATH"
    clang -g -S -c -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names -I/e/include /e/cases/c01_holds.c -o f.ll
    opt -S -p=mem2reg f.ll -o f.ll
    /work/build/wrapper/verifier -ae-sparsity=dense f.ll
  '
```

See [REPORT.md](REPORT.md) for the full evaluation command and [SCHEMA.md](SCHEMA.md) for the report format.
