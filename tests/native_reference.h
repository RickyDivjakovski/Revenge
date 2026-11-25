#ifndef REVENGE_NATIVE_REFERENCE_H
#define REVENGE_NATIVE_REFERENCE_H
#include <stdint.h>
#if defined(_WIN32)
# if defined(REVENGE_REFERENCE_BUILD)
#  define REF_API __declspec(dllexport)
# else
#  define REF_API __declspec(dllimport)
# endif
#else
# define REF_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
REF_API uint32_t ref_step(uint32_t increment, uint32_t* value);
REF_API uint32_t ref_unwind(uint32_t value, uint32_t* result);
REF_API uint32_t ref_retired(void);
#ifdef __cplusplus
}
#endif
#endif
