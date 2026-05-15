/* MSVC doesn't support C99 variable-length arrays (in either C or C++
   mode). RNNoise uses VLAs in celt_lpc.c and pitch.c. This shim maps
   them to _alloca on MSVC and to native VLAs elsewhere so upstream
   diffs stay tiny.

   Sizes used in RNNoise's VLAs are bounded by FRAME_SIZE / PITCH_*
   constants (at most a few hundred elements), so stack allocation via
   _alloca is safe — no need for _malloca/_freea pairing. */

#ifndef RNNOISE_VLA_SHIM_H
#define RNNOISE_VLA_SHIM_H

#ifdef _MSC_VER
#include <malloc.h>
#define VLA_DECL(type, name, count) type *name = (type*)_alloca((count) * sizeof(type))
#else
#define VLA_DECL(type, name, count) type name[(count)]
#endif

#endif
