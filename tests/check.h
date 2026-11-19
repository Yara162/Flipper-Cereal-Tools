// Minimal test helpers for the host tests.
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int check_failures;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if(!(cond)) {                                                               \
            printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);          \
            check_failures++;                                                       \
        }                                                                           \
    } while(0)

#define CHECK_EQ_BYTES(a, alen, b, blen)                                            \
    do {                                                                            \
        size_t _al = (alen), _bl = (blen);                                          \
        if(_al != _bl || memcmp((a), (b), _al) != 0) {                              \
            printf("%s:%d: bytes differ (%zu vs %zu):", __FILE__, __LINE__, _al, _bl); \
            for(size_t _i = 0; _i < _al; _i++) printf(" %02X", ((const uint8_t*)(a))[_i]); \
            printf(" |");                                                           \
            for(size_t _i = 0; _i < _bl; _i++) printf(" %02X", ((const uint8_t*)(b))[_i]); \
            printf("\n");                                                           \
            check_failures++;                                                       \
        }                                                                           \
    } while(0)

#define RUN(test)                                                                   \
    do {                                                                            \
        int _before = check_failures;                                               \
        test();                                                                     \
        if(check_failures != _before) printf("  in %s\n", #test);                   \
    } while(0)

#define DONE(name)                                                                  \
    do {                                                                            \
        if(check_failures) {                                                        \
            printf("%s: %d check(s) failed\n", name, check_failures);               \
            return 1;                                                               \
        }                                                                           \
        printf("%s: all passed\n", name);                                           \
        return 0;                                                                   \
    } while(0)
