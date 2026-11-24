#ifndef SVF_AE_VERIFIER_H
#define SVF_AE_VERIFIER_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void __verifier_assert(int condition, int line);
void __verifier_eq(int equal, long long lhs, long long rhs, int line);
void __verifier_unreachable(int line);

int verifier_nondet_int(void);
unsigned verifier_nondet_uint(void);
long long verifier_nondet_llong(void);
char verifier_nondet_char(void);
#ifdef __cplusplus
bool verifier_nondet_bool(void);
}
#else
_Bool verifier_nondet_bool(void);
#endif

#define verifier_assert(condition) __verifier_assert(!!(condition), __LINE__)
#define verifier_unreachable() __verifier_unreachable(__LINE__)
#define verifier_assume(condition) do { if (!(condition)) abort(); } while (0)

#ifdef __cplusplus
#define SVF_VERIFIER_AUTO auto
#else
#define SVF_VERIFIER_AUTO __auto_type
#endif

/* Evaluate each operand once and compare using the usual C conversions. */
#define verifier_eq(lhs, rhs) do { \
    SVF_VERIFIER_AUTO svf_verifier_lhs = (lhs); \
    SVF_VERIFIER_AUTO svf_verifier_rhs = (rhs); \
    __verifier_eq(svf_verifier_lhs == svf_verifier_rhs, \
                  (long long)svf_verifier_lhs, (long long)svf_verifier_rhs, __LINE__); \
} while (0)

#endif
