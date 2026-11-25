# Properties

Both properties decide sites from SVF's final abstract execution state and the associated SVF IR. The verdicts are `proved` and `unproved`. An unproved site may satisfy the property. A reason describes the evidence AE exposed. A property is proved when all its sites are proved, including when the property has no sites.

## Reach

Include `verifier.h` and use these C macros and declarations. The assertion macros evaluate their arguments once. The checks are external declarations for analysis, so producing LLVM IR does not require a runtime.

| Spelling | Meaning |
|---|---|
| `verifier_assert(expr)` | Check that `expr` is true. The macro passes a normalized Boolean and its source line to `__verifier_assert`. |
| `verifier_eq(lhs, rhs)` | Check equality using C conversions. The macro passes the comparison, operands converted to `long long`, and the source line to `__verifier_eq`. |
| `verifier_unreachable()` | Mark an error site via `__verifier_unreachable`. |
| `reach_error()` | A direct call marks an error site, using the debug location. |
| `verifier_assume(expr)` | Continue only when `expr` holds. The macro lowers to a conditional branch with `abort()` on the false arm, since pinned SVF has no `__verifier_assume` intrinsic model. |
| `verifier_nondet_int()`, `verifier_nondet_uint()`, `verifier_nondet_llong()`, `verifier_nondet_char()`, `verifier_nondet_bool()` | External declarations for arbitrary inputs of the indicated types. |

| Final state at the site | Verdict | Reason |
|---|---|---|
| No recorded state | unproved | `ae-no-state` |
| An error site has a recorded state | unproved | `ae-reachable` |
| Assertion or equality condition is `[0, 0]` | unproved | `ae-must-fail-if-reached` |
| Assertion condition is `[1, 1]` | proved | `ae-condition-true` |
| Equality condition is `[1, 1]` and both operand intervals are the same finite singleton | proved | `ae-condition-true` |
| Other assertion or equality evidence | unproved | `ae-condition-unknown` |

The reach property does not infer unreachability from missing states. Assertions are observations in the verifier and do not constrain later checks.

The assume macro expresses a precondition in the input program. AE can still lose the condition when analyzing the continuation.

## Overflow

The sites are loads and stores represented by SVF statements and individual accesses at direct calls classified by SVF's `MEMCPY`, `MEMSET`, `STRCPY`, or `STRCAT` annotations. Recognized copies and concatenations have a destination write and a source read, and fills have a destination write. Annotated LLVM intrinsics follow the same rules. The verifier excludes synthetic call and return statements as individual load/store sites.

The verifier checks `[offset, offset + length)` against `[0, size)` for every abstract target. A load or store uses the SVF byte size of its value type. A counted byte call uses the AE argument interval, with a maximum alone where the access can stop early. A proof uses the largest possible length and smallest possible object size. A definite violation requires a positive minimum length and a byte outside the largest possible object for every modeled offset.

The pinned AE string-length query can create field objects. The verifier leaves string copy and concatenation lengths unknown, including the wide variants classified by SVF. Wide counts without a sound byte-length model and indirect length arguments stay unknown. A routine without a relevant SVF annotation, such as `wcsncpy` at the pinned revision, has no call site.

Offsets are available for direct base addresses and single GEPs whose source is the direct address of one object. Sequential steps use the index interval at the GEP and the element's allocation stride. Struct steps use the field's byte offset from the target layout, including padding. Arithmetic keeps negative offsets and infinities and leaves potentially wrapping finite offsets unknown. For example, the dense, semi-sparse, and sparse fixtures leave a guarded external index unproved when AE supplies no useful bounds for the GEP index.

Chained GEPs, pointers obtained through loads, and GEPs with ambiguous bases or unsupported layouts stay unknown. This includes multidimensional source accesses when the compiler emits a chain instead of a single multistep GEP. Sizes are available for fixed-size non-heap objects.

| Final state at the site | Verdict | Reason |
|---|---|---|
| No recorded state | unproved | `ae-no-state` |
| AE exposes no resolvable object | unproved | `ae-object-unknown` |
| A call has no usable upper bound on length and the analysis cannot establish a definite violation | unproved | `ae-length-unknown` |
| A possible target has a modeled access range entirely outside the allowed bounds or crossing a bound for all its modeled offsets | unproved | `ae-out-of-bounds-if-reached` |
| Every modeled target's full access range fits | proved | `ae-in-bounds` |
| Other bounds evidence, including missing offsets, sizes or widths. Any heap target makes a proof unavailable. | unproved | `ae-bounds-unknown` |
