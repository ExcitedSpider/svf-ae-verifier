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

The sites are loads and stores represented by SVF statements. Synthetic effects at call and return nodes are excluded. The verifier checks `[offset, offset + width)` against `[0, size)` for every abstract target. Width is the SVF byte size of the loaded or stored type.

Offsets are available for direct base addresses and some single constant GEPs from direct base addresses. SVF's constant helper saturates at the field limit and omits struct padding. The verifier leaves offsets at the limit, struct GEPs, dynamic GEPs and chained GEPs unknown. Sizes are available for fixed-size non-heap objects.

| Final state at the site | Verdict | Reason |
|---|---|---|
| No recorded state | unproved | `ae-no-state` |
| AE exposes no resolvable object | unproved | `ae-object-unknown` |
| A possible target has a modeled access range entirely outside the allowed bounds or crossing a bound for all its modeled offsets | unproved | `ae-out-of-bounds-if-reached` |
| Every modeled target's full access range fits | proved | `ae-in-bounds` |
| Other bounds evidence, including missing offsets, sizes or widths. Any heap target makes a proof unavailable. | unproved | `ae-bounds-unknown` |
