// The model's units (nulldc3 refsw3/refcore: pvr_isp_depth.h, pvr_tsp_plane.h) take corners and attributes as the
// TA gives them: the bits of IEEE singles. The probes and generators here work in floats; F32s(f, n) is the array of
// the bits of f[0 .. n - 1] (up to 5), alive for the call it is made in; F32s(nullptr) is a null pointer.
#pragma once
#include <cstdint>
#include <cstring>

struct F32s {
    uint32_t b[5] = {};
    bool null = false;
    F32s(const float* f, int n = 3) { if (f) memcpy(b, f, 4 * n); else null = true; }
    operator const uint32_t*() const { return null ? nullptr : b; }
};
