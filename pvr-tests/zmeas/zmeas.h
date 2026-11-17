/* What the generator and the reader of zmeas share: the probe, a flat quad over a whole cell, and what the
   depth unit stores for it (refsw2's pvr_isp_depth.h, through ../../model/gen_common.h). */
#pragma once
#include "gen_common.h"

namespace zmeas {

static const int P = 16;            // pages: the values of a ladder

// The triangle of the full-cell quad (strip TL, TR, BL, BR) that holds the pixel, in rasterizer order.
// false on the diagonal.
inline bool QuadTri(int cell, double px, double py, double x[3], double y[3])
{
    const double cx = gen::CellX(cell), cy = gen::CellY(cell), s = (px - cx) + (py - cy);
    if (s == 32.0)
        return false;
    if (s < 32.0) {
        x[0] = cx; x[1] = cx + 32; x[2] = cx;
        y[0] = cy; y[1] = cy; y[2] = cy + 32;
    } else {
        x[0] = cx; x[1] = cx + 32; x[2] = cx + 32;
        y[0] = cy + 32; y[1] = cy; y[2] = cy + 32;
    }
    return true;
}

// The stored depth of a full-cell flat quad of depth z at the pixel (which is not on the diagonal).
inline double QuadDepth(const gen::Model& m, int cell, double px, double py, double z)
{
    double x[3], y[3];
    const double zs[3] = { z, z, z };
    QuadTri(cell, px, py, x, y);
    return gen::IspDepth(m, x, y, zs, px, py);
}

} // namespace zmeas
