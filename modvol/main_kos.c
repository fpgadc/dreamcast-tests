/* Modifier volumes.

   Each cell has a polygon over the whole cell at z = 0.5 and modifier triangles ("sheets") over its left
   24 pixels. A pixel is inside a volume when an odd number of the volume's sheets are in front of it.

   Column = arrangement of sheets (z of each; 0.75 is in front of the polygon, 0.25 behind):
       0  one sheet in front              1  one sheet behind
       2  two sheets in front             3  one in front, one behind
       4  three sheets in front           5  two separate volumes, one sheet in front each
       6  one sheet in front, closed as an exclusion volume
       7  no sheets
   Row = kind of polygon:
       0  opaque, cheap shadow            1  opaque, two-volume (color 0x4080c0 outside, 0xc04020 inside)
       2  opaque, modifiers disabled      3-5  the same three in the translucent list
   Pages 0, 1, 2: cheap shadow intensity register 0x180, 0x100, 0x1ff. Page 3: 0x080 (intensity mode off). */
#define NUM_PAGES 4
#define TEST_MODIFIERS
#include "pvrtest.h"

static const uint32_t shadow_reg[NUM_PAGES] = { 0x180, 0x100, 0x1ff, 0x080 };

static void test_setup(void) {
}

static void test_describe(void) {
    static const char *arr[8] = { "1 front", "1 behind", "2 front", "front + behind", "3 front", "2 volumes", "1 front, exclude",
                                  "none" };
    static const char *kind[6] = { "OP cheap shadow", "OP two-volume", "OP no modifiers", "TR cheap shadow", "TR two-volume", "TR no modifiers" };
    int p, r, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 6; r++)
            for(c = 0; c < 8; c++)
                label(p, r * COLS + c, "reg %03x, %s, %s", (unsigned)shadow_reg[p], kind[r], arr[c]);
}

static void sheet(pvr_list_t list, int mode, float x, float y, float z) {
    pvr_mod_hdr_t hdr;
    pvr_modifier_vol_t v;
    int k;

    /* a 24x32 rectangle as two triangles; the last triangle carries the volume's mode */
    for(k = 0; k < 2; k++) {
        pvr_mod_compile(&hdr, list, k ? mode : PVR_MODIFIER_OTHER_POLY, PVR_CULLING_NONE);
        pvr_prim(&hdr, sizeof(hdr));
        memset(&v, 0, sizeof(v));
        v.flags = PVR_CMD_VERTEX_EOL;
        v.ax = x;          v.ay = y;          v.az = z;
        v.bx = x + 24.0f;  v.by = k ? y + 32.0f : y;  v.bz = z;
        v.cx = k ? x : x + 24.0f;  v.cy = y + 32.0f;  v.cz = z;
        pvr_prim(&v, sizeof(v));
    }
}

static void volumes(pvr_list_t list, int row) {
    int c;

    for(c = 0; c < 7; c++) {
        float x = cell_x(row * COLS + c), y = cell_y(row * COLS + c);
        int inc = PVR_MODIFIER_INCLUDE_LAST_POLY;
        switch(c) {
            case 0: sheet(list, inc, x, y, 0.75f); break;
            case 1: sheet(list, inc, x, y, 0.25f); break;
            case 2: sheet(list, PVR_MODIFIER_OTHER_POLY, x, y, 0.75f); sheet(list, inc, x, y, 0.8f); break;
            case 3: sheet(list, PVR_MODIFIER_OTHER_POLY, x, y, 0.75f); sheet(list, inc, x, y, 0.25f); break;
            case 4: sheet(list, PVR_MODIFIER_OTHER_POLY, x, y, 0.75f); sheet(list, PVR_MODIFIER_OTHER_POLY, x, y, 0.8f);
                    sheet(list, inc, x, y, 0.85f); break;
            case 5: sheet(list, inc, x, y, 0.75f); sheet(list, inc, x, y, 0.8f); break;
            case 6: sheet(list, PVR_MODIFIER_EXCLUDE_LAST_POLY, x, y, 0.75f); break;
        }
    }
}

static void polys(pvr_list_t list, int first_row) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_pcm_t v;
    int k, c, i;

    for(k = 0; k < 3; k++) {
        cxt_plain(&cxt, list);
        cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
        cxt.gen.shading = PVR_SHADE_FLAT;
        if(k < 2) {
            cxt.fmt.modifier = PVR_MODIFIER_ENABLE;
            cxt.gen.modifier_mode = k ? PVR_MODIFIER_NORMAL : PVR_MODIFIER_CHEAP_SHADOW;
        }
        pvr_poly_compile(&hdr, &cxt);
        for(c = 0; c < 8; c++) {
            float x = cell_x((first_row + k) * COLS + c), y = cell_y((first_row + k) * COLS + c);
            if(k != 1) {
                quad(&hdr, x, y, 32.0f, 32.0f, 0.5f, 0xff4080c0);
                continue;
            }
            pvr_prim(&hdr, sizeof(hdr));
            memset(&v, 0, sizeof(v));
            v.z = 0.5f;
            v.argb0 = 0xff4080c0;
            v.argb1 = 0xffc04020;
            for(i = 0; i < 4; i++) {
                v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = x + (i & 1) * 32.0f;
                v.y = y + (i >> 1) * 32.0f;
                pvr_prim(&v, sizeof(v));
            }
        }
    }
}

static void test_page(int page) {
    int r = 0;
    (void)r;

    PVR_SET(PVR_CHEAP_SHADOW, shadow_reg[page]);
    pvr_list_begin(PVR_LIST_OP_POLY);
    polys(PVR_LIST_OP_POLY, 0);
    pvr_list_finish();
#ifndef NO_OP_MOD
    pvr_list_begin(PVR_LIST_OP_MOD);
    for(r = 0; r < 3; r++)
        volumes(PVR_LIST_OP_MOD, r);
    pvr_list_finish();
#endif
    pvr_list_begin(PVR_LIST_TR_POLY);
    polys(PVR_LIST_TR_POLY, 3);
    pvr_list_finish();
#ifdef WITH_TR_MOD      /* submitting this list gives a black frame on hardware, see README */
    pvr_list_begin(PVR_LIST_TR_MOD);
    for(r = 3; r < 6; r++)
        volumes(PVR_LIST_TR_MOD, r);
    pvr_list_finish();
#endif
}
