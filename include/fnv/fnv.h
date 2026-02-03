/*
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org>
 *
 */

#ifndef FNV_H
#define FNV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#if !defined(FNV_IMPLEMENTATION)
#define FNV_EXTERN extern
#else
#define FNV_EXTERN
#endif

#if !defined(FNV_NO_GCC_OPTIMIZATION) && defined(__GNUC__) &&                  \
    !defined(__clang__)
#define FNV_GCC_OPTIMIZATION
#endif

#define FNV_32_1A_INIT ((uint32_t)0x811c9dc5)

#define FNV_64_1A_INIT ((uint64_t)0xcbf29ce484222325ULL)

FNV_EXTERN uint32_t FNV_hashNextBuffer32_1a(void *buffer, size_t size,
                                            uint32_t hash);

#define FNV_hashBuffer32_1a(buffer, size)                                      \
  FNV_hashNextBuffer32_1a((buffer), (size), FNV_32_1A_INIT)

FNV_EXTERN uint32_t FNV_hashNextCstr32_1a(char *cstr, uint32_t hash);

#define FNV_hashCstr32_1a(cstr) FNV_hashNextCstr32_1a((cstr), FNV_32_1A_INIT)

FNV_EXTERN uint64_t FNV_hashNextBuffer64_1a(void *buffer, size_t size,
                                            uint64_t hash);

#define FNV_hashBuffer64_1a(buffer, size)                                      \
  FNV_hashNextBuffer64_1a((buffer), (size), FNV_32_1A_INIT)

FNV_EXTERN uint64_t FNV_hashNextCstr64_1a(char *cstr, uint64_t hash);

#define FNV_hashCstr64_1a(cstr) FNV_hashNextCstr64_1a((cstr), FNV_32_1A_INIT)

#if defined(FNV_IMPLEMENTATION)

#define FNV_32_1A_PRIME ((uint32_t)0x01000193)

uint32_t FNV_hashNextBuffer32_1a(void *buffer, size_t size, uint32_t hash) {
  // NOTE: this is based on code from:
  // https://github.com/lcn2/fnv/blob/6f5d7fa29f92987311223e71ecf8b13f7c5551f2/hash_32a.c#L103

  unsigned char *bp = (unsigned char *)buffer; /* start of buffer */
  unsigned char *be = bp + size;               /* beyond end of buffer */

  /*
   * FNV-1a hash each octet in the buffer
   */
  while (bp < be) {

    /* xor the bottom with the current octet */
    hash ^= (uint32_t)*bp++;

/* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(FNV_GCC_OPTIMIZATION)
    hash +=
        (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
#else
    hash *= FNV_32_1A_PRIME;
#endif
  }

  /* return our new hash value */
  return hash;
}

uint32_t FNV_hashNextCstr32_1a(char *cstr, uint32_t hash) {
  // NOTE: this is based on code from:
  // https://github.com/lcn2/fnv/blob/6f5d7fa29f92987311223e71ecf8b13f7c5551f2/hash_32a.c#L143

  unsigned char *s = (unsigned char *)cstr; /* unsigned string */

  /*
   * FNV-1a hash each octet in the buffer
   */
  while (*s) {

    /* xor the bottom with the current octet */
    hash ^= (uint32_t)*s++;

/* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(FNV_GCC_OPTIMIZATION)
    hash +=
        (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
#else
    hash *= FNV_32_1A_PRIME;
#endif
  }

  /* return our new hash value */
  return hash;
}

#define FNV_64_1A_PRIME ((uint64_t)0x100000001b3ULL)

uint64_t FNV_hashNextBuffer64_1a(void *buffer, size_t size, uint64_t hash) {
  // NOTE: this is based on code from:
  // https://github.com/lcn2/fnv/blob/6f5d7fa29f92987311223e71ecf8b13f7c5551f2/hash_64a.c#L116

  unsigned char *bp = (unsigned char *)buffer; /* start of buffer */
  unsigned char *be = bp + size;               /* beyond end of buffer */

  /*
   * FNV-1a hash each octet of the buffer
   */
  while (bp < be) {

    /* xor the bottom with the current octet */
    hash ^= (uint64_t)*bp++;

#if defined(FNV_GCC_OPTIMIZATION)
    /* multiply by the 64 bit FNV magic prime mod 2^64 */
    hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) +
            (hash << 8) + (hash << 40);
#else
    hash *= FNV_64_1A_PRIME;
#endif
  }

  /* return our new hash value */
  return hash;
}

uint64_t FNV_hashNextCstr64_1a(char *cstr, uint64_t hash) {
  // NOTE: this is based on code from:
  // https://github.com/lcn2/fnv/blob/6f5d7fa29f92987311223e71ecf8b13f7c5551f2/hash_64a.c#L222

  unsigned char *s = (unsigned char *)cstr; /* unsigned string */
  /*
   * FNV-1a hash each octet of the string
   */
  while (*s) {

    /* xor the bottom with the current octet */
    hash ^= (uint64_t)*s++;

/* multiply by the 64 bit FNV magic prime mod 2^64 */
#if defined(FNV_GCC_OPTIMIZATION)
    /* multiply by the 64 bit FNV magic prime mod 2^64 */
    hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) +
            (hash << 8) + (hash << 40);
#else
    hash *= FNV_64_1A_PRIME;
#endif
  }

  /* return our new hash value */
  return hash;
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* FNV_H */
