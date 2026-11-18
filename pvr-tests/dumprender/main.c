/* A dump of a game's frame (dreamster's polly2-data: pvr_regs_<scene>.bin, the 8 KB of 0x005F8000-0x005F9FFF,
   and vram_<scene>.bin, video memory in 32 bit area order; or <scene>.pvrdmp, both in one PVR core dump) rendered by
   refsw3-offline and by a console, with the frame or a part of it: the same input on both sides, and the framebuffer
   dumped from both.

   Built by ../../dumprender.sh, which gives the defines:
     DUMPS     the folder of the dumps (on a console read through dc-load's /pc)
     SCENE     the scene's name
     PVRDMP    the dump is <scene>.pvrdmp (nulldc3 libswirl/hw/pvr/pvr_coredump.h: "pvrcoredump", a gzip member of
               video memory, a gzip member of the registers): both sides inflate it (common/gunzip.h); the console
               gets it inside the program, compressed, and the PC's refsw3-offline loads it with its pvrdmp command
     TILES     optional: { x0, y0, x1, y1, ... } and NTILES: only the region array entries of these tiles
               (in their order in the array), the rest left out: a partial render
     DROP      optional: { t0, t1, ... }: the objects of these refsw2 tags (as its render log prints them: the
               parameter offset in words in bits 3-23) are left out of every list of every entry kept: their
               object list entries become strips with no triangle enabled
     NOLISTS   optional: a mask of the lists to leave out of every entry kept (1 opaque, 2 opaque modifier
               volumes, 4 translucent, 8 translucent modifier volumes, 16 punch through): their pointers are emptied
     POKES     optional: { address, value, ... }: words of video memory (32 bit area) written after the dump's
     SETREGS   optional: { offset, value, ... }: registers written after the dump's (e.g. SCALER_CTL 0x400, so that
               the framebuffer is the tiles as they were shaded, without the vertical filter)
     ZEXFIL    optional: the depth buffer read out instead (common/zexfil.h; below), of every tile or of TILES
   Output: frame.off.raw (PC) / frame.hw.raw (console): the framebuffer FB_W_SOF1 .. FB_Y_CLIP's last line.

   On a console (pvr-renderer's rules, polly2-rtl): the core is reset before video memory is written (a render
   still running starves the CPU's accesses to it and hangs the console), and the whole register file is
   written except SOFTRESET and STARTRENDER, twice. refsw2-offline loads both raw (`regs`, `vram`). */
#define NUM_PAGES 0
#include "pvrio.h"

#ifndef DUMPS
#error "DUMPS: build with dumprender.sh"
#endif
#ifndef SCENE
#error "SCENE: build with dumprender.sh"
#endif
#ifdef TILES
static const int tiles[] = TILES;
#endif
#ifdef DROP
static const uint32_t drop[] = DROP;
#endif

/* a message: on a PC stdout is refsw2-offline's input, so there it is an echo command */
#ifdef _arch_dreamcast
#define SAY "dumprender: "
#else
#define SAY "echo dumprender: "
#endif

#define VRAM_BYTES (8u << 20)
#define REG_BYTES  8192u

static uint32_t regs[REG_BYTES / 4];
static uint32_t chunk[16384];

/* On a console the dump is part of the program: dc-load sends a program at about 13 MB/s, where reading the
   files through /pc takes minutes. A .pvrdmp is inflated into memory on both sides (DR_IN_MEMORY: the dump's
   words come from dr_vram, dr_regs there, not from the files) */
#if defined(_arch_dreamcast) || defined(PVRDMP)
#define DR_IN_MEMORY 1
#else
#define DR_IN_MEMORY 0
#endif
#define DR_STR2(x) #x
#define DR_STR(x) DR_STR2(x)
#define DR_P DR_STR(__USER_LABEL_PREFIX__)    /* sh-elf: C names have a leading underscore */
#ifdef PVRDMP
#include <stdlib.h>                     /* malloc */
#include "gunzip.h"
static uint32_t dr_vram_buf[VRAM_BYTES / 4], dr_regs_buf[REG_BYTES / 4];
static const uint32_t *dr_regs, *dr_vram, *dr_vram_end;
#ifdef _arch_dreamcast
__asm__(".section .rodata\n.balign 32\n.global " DR_P "dr_pvrdmp\n" DR_P "dr_pvrdmp:\n.incbin \"" DUMPS "/" SCENE ".pvrdmp\"\n"
        ".global " DR_P "dr_pvrdmp_end\n" DR_P "dr_pvrdmp_end:\n.section .text\n");
extern const uint8_t dr_pvrdmp[], dr_pvrdmp_end[];
#endif

/* the dump inflated: video memory and the registers */
static int unpack(void) {
    const uint8_t *in;
    size_t len, pos = 11;
    long n;
#ifdef _arch_dreamcast
    in = dr_pvrdmp;
    len = (size_t)(dr_pvrdmp_end - dr_pvrdmp);
#else
    static uint8_t *file;
    FILE *f = fopen(DUMPS "/" SCENE ".pvrdmp", "rb");
    if(!f) {
        printf(SAY "cannot open %s\n", DUMPS "/" SCENE ".pvrdmp");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    file = (uint8_t *)malloc(len);
    if(!file || fread(file, 1, len, f) != len)
        len = 0;
    fclose(f);
    in = file;
#endif
    if(len < 11 || memcmp(in, "pvrcoredump", 11)) {
        printf(SAY "%s.pvrdmp is not a PVR core dump\n", SCENE);
        return 0;
    }
    n = gz_member(in, len, &pos, (uint8_t *)dr_vram_buf, VRAM_BYTES);
    if(n != (long)VRAM_BYTES) {
        printf(SAY "%s.pvrdmp: no 8 MB of video memory (a NAOMI's 16 MB, or a broken file)\n", SCENE);
        return 0;
    }
    if(gz_member(in, len, &pos, (uint8_t *)dr_regs_buf, REG_BYTES) != (long)REG_BYTES) {
        printf(SAY "%s.pvrdmp: no 8 KB of registers\n", SCENE);
        return 0;
    }
    dr_regs = dr_regs_buf;
    dr_vram = dr_vram_buf;
    dr_vram_end = dr_vram_buf + VRAM_BYTES / 4;
    return 1;
}
#elif defined(_arch_dreamcast)
__asm__(".section .rodata\n.balign 32\n.global " DR_P "dr_regs\n" DR_P "dr_regs:\n.incbin \"" DUMPS "/pvr_regs_" SCENE ".bin\"\n"
        ".balign 32\n.global " DR_P "dr_vram\n" DR_P "dr_vram:\n.incbin \"" DUMPS "/vram_" SCENE ".bin\"\n"
        ".global " DR_P "dr_vram_end\n" DR_P "dr_vram_end:\n.section .text\n");
extern const uint32_t dr_regs[], dr_vram[], dr_vram_end[];
#endif

static FILE *open_dump(const char *what) {
    char path[400];
#ifdef _arch_dreamcast
    snprintf(path, sizeof(path), "/pc%s/%s_%s.bin", DUMPS, what, SCENE);
#else
    snprintf(path, sizeof(path), "%s/%s_%s.bin", DUMPS, what, SCENE);
#endif
    return fopen(path, "rb");
}

/* registers and video memory, into the checksum on both sides; the console also writes them */
static int load(void) {
    uint32_t at;
    int n;
#ifdef PVRDMP
    if(!unpack())
        return 0;
#endif
#ifdef _arch_dreamcast
    memcpy(regs, dr_regs, REG_BYTES);
    *(volatile uint32_t *)0xa05f8008 = 3;           /* core and TA reset: nothing may be touching video memory */
    *(volatile uint32_t *)0xa05f8008 = 0;
    pvrio_hash('G', 0, regs, REG_BYTES / 4);
    for(at = 0; at < VRAM_BYTES && dr_vram + at / 4 < dr_vram_end; at += (uint32_t)n * 4) {
        volatile uint32_t *dst = (volatile uint32_t *)(0xa5000000 + at);
        const uint32_t *src = dr_vram + at / 4;
        int i;
        n = (int)(sizeof(chunk) / 4);
        if(src + n > dr_vram_end)
            n = (int)(dr_vram_end - src);
        pvrio_hash('V', at, src, n);
        for(i = 0; i < n; i++)
            dst[i] = src[i];
    }
#elif defined(PVRDMP)
    memcpy(regs, dr_regs, REG_BYTES);
    printf("pvrdmp %s/%s.pvrdmp\n", DUMPS, SCENE);
    pvrio_hash('G', 0, regs, REG_BYTES / 4);
    for(at = 0; at < VRAM_BYTES; at += (uint32_t)n * 4) {        /* in the console's pieces: the same checksum */
        n = (int)(sizeof(chunk) / 4);
        pvrio_hash('V', at, dr_vram + at / 4, n);
    }
#else
    FILE *f = open_dump("pvr_regs");
    if(!f || fread(regs, 1, REG_BYTES, f) != REG_BYTES) {
        printf(SAY "cannot read the 8 KB of registers of %s\n", SCENE);
        return 0;
    }
    fclose(f);
    printf("regs %s/pvr_regs_%s.bin\nvram %s/vram_%s.bin\n", DUMPS, SCENE, DUMPS, SCENE);
    pvrio_hash('G', 0, regs, REG_BYTES / 4);
    f = open_dump("vram");
    if(!f) {
        printf(SAY "cannot open the video memory of %s\n", SCENE);
        return 0;
    }
    for(at = 0; at < VRAM_BYTES; at += (uint32_t)n * 4) {
        n = (int)fread(chunk, 4, sizeof(chunk) / 4, f);
        if(n <= 0)
            break;
        pvrio_hash('V', at, chunk, n);
    }
    fclose(f);
#endif
#ifdef _arch_dreamcast
    {
        int pass;
        uint32_t i;
        for(pass = 0; pass < 2; pass++) {
            for(i = 0; i < REG_BYTES / 4; i++)
                if(i * 4 != 0x008 && i * 4 != 0x014)
                    *(volatile uint32_t *)(0xa05f8000 + i * 4) = regs[i];
            thd_sleep(5);
        }
    }
#endif
    return 1;
}

/* words of the dump's video memory (32 bit area): the embedded copy on a console, the file on a PC */
static void dump_words(uint32_t addr, uint32_t *out, int n) {
#if DR_IN_MEMORY
    memcpy(out, dr_vram + addr / 4, 4 * (size_t)n);
#else
    FILE *f = open_dump("vram");
    fseek(f, (long)addr, SEEK_SET);
    if(fread(out, 4, (size_t)n, f) != (size_t)n)
        memset(out, 0, 4 * (size_t)n);
    fclose(f);
#endif
}

#ifdef DROP
/* every object of the lists that start at these pointers: entries of dropped parameters become empty strips */
static void drop_objects(const uint32_t *ptrs, int n_ptrs) {
    static uint32_t seen[8192];
    static int n_seen;
    int k;
    for(k = 0; k < n_ptrs; k++) {
        uint32_t at = ptrs[k] & 0xfffffcu, w;
        int steps = 0, j;
        if(ptrs[k] >> 31)
            continue;                                   /* the list is empty */
        while(steps++ < 100000) {
            dump_words(at, &w, 1);
            if(w >> 31 && ((w >> 29) & 7) == 7) {       /* a link, or the end */
                if((w >> 28) & 1)
                    break;
                at = w & 0xfffffcu;
                continue;
            }
            for(j = 0; j < (int)(sizeof(drop) / 4); j++)
                if((w & 0x1fffff) == ((drop[j] >> 3) & 0x1fffff)) {
                    uint32_t e = w & 0x1fffff;          /* a strip of nothing */
                    int q;
                    for(q = 0; q < n_seen && seen[q] != at; q++) ;
                    if(q == n_seen && n_seen < 8192) {
                        seen[n_seen++] = at;
                        pvrio_w32(at, &e, 1);
                    }
                    break;
                }
            at += 4;
        }
    }
}
#endif

#ifdef TILES
/* the region array with only the entries of the tiles asked for */
static void keep_tiles(void) {
    static uint32_t in[6 * 4096], out[6 * 4096];
    const uint32_t base = regs[REGION_BASE_addr / 4] & 0xfffffcu;
    const int size = (regs[FPU_PARAM_CFG_addr / 4] >> 21) & 1 ? 6 : 5;
    int i, k, n_in = 0, n_out = 0;
#if DR_IN_MEMORY
    memcpy(in, dr_vram + base / 4, sizeof(in));
    n_in = (int)(sizeof(in) / 4) / size;
#else
    FILE *f = open_dump("vram");
    fseek(f, (long)base, SEEK_SET);
    n_in = (int)fread(in, 4, sizeof(in) / 4, f) / size;
    fclose(f);
#endif
    for(i = 0; i < n_in; i++) {
        const uint32_t c = in[size * i];
        const int x = (int)((c >> 2) & 63), y = (int)((c >> 8) & 63);
        for(k = 0; k < NTILES; k++)
            if(tiles[2 * k] == x && tiles[2 * k + 1] == y) {
                memcpy(&out[size * n_out], &in[size * i], 4 * (size_t)size);
                out[size * n_out] &= 0x7fffffffu;
#ifdef NOLISTS
                {
                    int l;
                    for(l = 0; l < size - 1; l++)
                        if((NOLISTS >> l) & 1)
                            out[size * n_out + 1 + l] = 0x80000000u;
                }
#endif
                n_out++;
                break;
            }
        if(c >> 31)
            break;
    }
    if(!n_out) {
        printf(SAY "no entry of those tiles\n");
        return;
    }
    out[size * (n_out - 1)] |= 0x80000000u;
    pvrio_w32(base, out, size * n_out);
    printf(SAY "%d of %d region array entries kept\n", n_out, i + 1);
#ifdef DROP
    for(i = 0; i < n_out; i++)
        drop_objects(&out[size * i + 1], size - 1);
#endif
}
#endif

#ifdef ZEXFIL
/* ---- ZEXFIL: the depth buffer of tiles read out (common/zexfil.h), instead of the frame ----

   Every tile of the region array (or the tiles of TILES) in turn: its entries, not written out, then zexfil's keep Z
   entry at tile (0, 0); 27 renders a tile. The probes need ZEXFIL_AREA bytes at or above PARAM_BASE's page that
   the render does not read: a render of the tile reads its region array entries, their object lists, the
   parameters of their entries, the textures those name, and the background; any other memory will do, and is put
   back from the dump after the tile. Tile (0, 0) is written out (0888, no scaling) in that area too, so the dump's
   own framebuffer is left alone. Output: depth.off.raw / depth.hw.raw, 640x480 words, each tile's depths at its
   place (and depthr: the tiles of x 20-39, of a frame rendered 1280 wide). */
#include <stdlib.h>                     /* calloc */
#include "zexfil.h"

static const uint32_t *dz_vram;         /* the dump's video memory, 32 bit area */
static uint32_t dz_vram_words;
static uint32_t dz_region[6 * 4096];    /* the dump's region array */
static int dz_entries, dz_ew;
static uint32_t dz_used[2 * 8192];      /* what a render of the tile reads: [start, end) pairs */
static int dz_n_used;

static uint32_t dz_word(uint32_t a) {
    a &= 0x7ffffcu;
    return a / 4 < dz_vram_words ? dz_vram[a / 4] : 0;
}

static void dz_use(uint32_t start, uint32_t end) {
    if(dz_n_used < 8192) {
        dz_used[2 * dz_n_used] = start;
        dz_used[2 * dz_n_used + 1] = end;
        dz_n_used++;
    }
}

/* a texture's memory from its TSP and TCW words (in both chips: the 64 bit area interleaves them) */
static void dz_texture(uint32_t tsp, uint32_t tcw) {
    const uint32_t stride = (regs[TEXT_CONTROL_addr / 4] & 31) * 32, fmt = (tcw >> 27) & 7;
    uint32_t w = 8u << ((tsp >> 3) & 7), h = 8u << (tsp & 7), size, a, lo, hi;
    if((tcw >> 26) & 1) {                           /* not twiddled: 16 bit texels, no mipmaps */
        if((tcw >> 25) & 1)
            w = stride;
        size = w * h * 2;
    } else {
        size = (tcw >> 30) & 1 ? 2048 + w * h / 4 : w * h * (fmt == 5 ? 4 : fmt == 6 ? 8 : 16) / 8;
        if(tcw >> 31)
            size += size / 3 + 64;                  /* mipmaps */
    }
    a = (tcw & 0xfffffu) << 3;
    lo = (a / 2) & ~3u;
    hi = (a + size + 7) / 2 + 4;
    dz_use(lo, hi);
    dz_use(lo + 0x400000, hi + 0x400000);
}

/* an object list: its words, the parameters of its entries (a strip taken as 8 vertices), their textures */
static void dz_walk(uint32_t at, int volume) {
    const uint32_t page = regs[PARAM_BASE_addr / 4] & 0xf00000u;
    uint32_t w, kind, skip, shadow, head, vsize, size, p;
    int steps = 0;
    while(steps++ < 65536) {
        w = dz_word(at);
        dz_use(at, at + 4);
        kind = w >> 29;
        if(kind == 7) {                             /* a link, or the end */
            if((w >> 28) & 1)
                break;
            at = w & 0xfffffcu;
            continue;
        }
        skip = (w >> 21) & 7;
        shadow = (w >> 24) & 1;
        head = volume ? 1 : 3 + 2 * shadow;         /* a modifier volume triangle: an ISP word and 3 corners */
        vsize = volume ? 3 : 3 + skip * (1 + shadow);
        size = !(w >> 31) ? head + 8 * vsize :
               kind == 4 || kind == 5 ? (((w >> 25) & 15) + 1) * (head + (kind == 4 ? 3 : 4) * vsize) : 0;
        if(size) {
            p = page + 4 * (w & 0x1fffff);
            dz_use(p, p + 4 * size);
            if(!volume && (dz_word(p) >> 25) & 1) {
                dz_texture(dz_word(p + 4), dz_word(p + 8));
                if(shadow)
                    dz_texture(dz_word(p + 12), dz_word(p + 16));
            }
        }
        at += 4;
    }
}

static int dz_entries_of(int x, int y, uint32_t *w) {
    int i, n = 0;
    for(i = 0; i < dz_entries && n < ZEXFIL_MAX_ENTRIES; i++) {
        const uint32_t c = dz_region[dz_ew * i];
        if((int)((c >> 2) & 63) == x && (int)((c >> 8) & 63) == y)
            memcpy(&w[dz_ew * n++], &dz_region[dz_ew * i], 4 * (size_t)dz_ew);
    }
    return n;
}

/* the memory a render of tile (x, y) reads; ZEXFIL_AREA bytes at or above PARAM_BASE's page outside it */
static uint32_t dz_area(int x, int y) {
    static uint32_t e[6 * ZEXFIL_MAX_ENTRIES];
    const uint32_t page = regs[PARAM_BASE_addr / 4] & 0xf00000u, bg = regs[ISP_BACKGND_T_addr / 4];
    uint32_t a, p;
    int n, i, k;

    dz_n_used = 0;
    n = dz_entries_of(x, y, e);
    for(i = 0; i < n; i++)
        for(k = 1; k < dz_ew; k++)
            if(!(e[dz_ew * i + k] >> 31))
                dz_walk(e[dz_ew * i + k] & 0xfffffcu, k == 2 || k == 4);
    p = page + 4 * ((bg >> 3) & 0x1fffff);
    dz_use(p, p + 256);
    if((dz_word(p) >> 25) & 1)
        dz_texture(dz_word(p + 4), dz_word(p + 8));
    for(a = page; a + ZEXFIL_AREA <= VRAM_BYTES; a += 0x10000) {
        for(i = 0; i < dz_n_used; i++)
            if(dz_used[2 * i] < a + ZEXFIL_AREA && dz_used[2 * i + 1] > a)
                break;
        if(i == dz_n_used)
            return a;
    }
    return 0xffffffffu;
}

static void dz_render(uint32_t region) {
    pvrio_reg(REGION_BASE_addr, region);
    pvrio_reg(STARTRENDER_addr, 0xffffffff);
    pvrio_wait(PVRIO_RENDER_DONE);
}

static void dz_run(void) {
    static uint32_t frame[2][640 * 480], z[1024];
    const uint32_t base = regs[REGION_BASE_addr / 4] & 0xfffffcu;
    int tiles_x[1280], tiles_y[1280], n_tiles = 0, used_right = 0, t, i, j, x, y;
    uint32_t area;
    char line[160];

    if(!(regs[HALF_OFFSET_addr / 4] & 1)) {
        printf(SAY "zexfil: HALF_OFFSET bit 0 is clear (samples at pixel corners); the probes are for centers\n");
        return;
    }
#if DR_IN_MEMORY
    dz_vram = dr_vram;
    dz_vram_words = (uint32_t)(dr_vram_end - dr_vram);
#else
    {
        static uint32_t *v;
        FILE *f = open_dump("vram");
        v = (uint32_t *)calloc(VRAM_BYTES / 4, 4);
        dz_vram_words = f ? (uint32_t)fread(v, 4, VRAM_BYTES / 4, f) : 0;
        if(f)
            fclose(f);
        dz_vram = v;
    }
#endif
    dz_ew = (regs[FPU_PARAM_CFG_addr / 4] >> 21) & 1 ? 6 : 5;
    for(dz_entries = 0; dz_entries < 4096; ) {
        for(i = 0; i < dz_ew; i++)
            dz_region[dz_ew * dz_entries + i] = dz_word(base + 4 * (uint32_t)(dz_ew * dz_entries + i));
        if(dz_region[dz_ew * dz_entries++] >> 31)
            break;
    }
#ifdef TILES
    for(t = 0; t < NTILES; t++) {
        tiles_x[n_tiles] = tiles[2 * t];
        tiles_y[n_tiles++] = tiles[2 * t + 1];
    }
#else
    for(i = 0; i < dz_entries && n_tiles < 1280; i++) {            /* every tile, in the array's order */
        x = (int)((dz_region[dz_ew * i] >> 2) & 63);
        y = (int)((dz_region[dz_ew * i] >> 8) & 63);
        for(t = 0; t < n_tiles && (tiles_x[t] != x || tiles_y[t] != y); t++) ;
        if(t == n_tiles && x < 40 && y < 15) {
            tiles_x[n_tiles] = x;
            tiles_y[n_tiles++] = y;
        }
    }
#endif
    zexfil_cfg.param_base = regs[PARAM_BASE_addr / 4];
    zexfil_cfg.entry_words = dz_ew;
    zexfil_cfg.stride = 128;
    zexfil_cfg.entries = dz_entries_of;
    zexfil_cfg.render = dz_render;
    pvrio_reg(FB_W_LINESTRIDE_addr, 128 / 8);
    pvrio_reg(FB_W_CTRL_addr, fb_packmode_0888);
    pvrio_reg(FB_X_CLIP_addr, 639u << 16);
    pvrio_reg(FB_Y_CLIP_addr, 479u << 16);
    pvrio_reg(SCALER_CTL_addr, 0x400);
    for(t = 0; t < n_tiles; t++) {
        x = tiles_x[t];
        y = tiles_y[t];
        area = dz_area(x, y);
        if(area == 0xffffffffu) {
            printf(SAY "zexfil: no memory for the probes of tile (%d, %d)\n", x, y);
            continue;
        }
        zexfil_cfg.area = area;
        zexfil_cfg.fb = zexfil_fb();
        pvrio_reg(FB_W_SOF1_addr, zexfil_fb());
        zexfil_tile(x, y, z);
        pvrio_w32(area, dz_vram + area / 4, (int)(ZEXFIL_AREA / 4));    /* the dump's memory back */
        used_right |= x >= 20;
        for(j = 0; j < 32; j++)
            memcpy(&frame[x >= 20][(y * 32 + j) * 640 + (x % 20) * 32], &z[j * 32], 32 * 4);
    }
    snprintf(line, sizeof(line), "%d tiles read out, %d readings of another colour than the probe's", n_tiles, zexfil_foreign);
    pvrio_hw_say(line);
    pvrio_unhashed = 1;                             /* the depths into video memory, to be dumped */
    for(i = 0; i <= used_right; i++) {
        pvrio_w32(0, frame[i], 640 * 480);
        pvrio_dump32(0, 640 * 480, i ? "depthr" : "depth", ".raw");
    }
    pvrio_unhashed = 0;
}
#endif

#ifdef _arch_dreamcast
/* The display registers (0x040 .. 0x0fc: border, FB_R_*, SPG_*, VO_*, SCALER_CTL) as KallistiOS set them: the dump's
   replace them (load writes the whole register file), and are taken back before the program ends. With a dump's
   interlaced scaler left in place (SCALER_CTL bit 17: nfl2k1_title, nfl2k1_spin) the console hung after the program
   had returned (2026-11-18), and took the next program down with it */
static uint32_t dr_display[0x100 / 4];

static void display_save(void) {
    int i;
    for(i = 0x040 / 4; i < 0x100 / 4; i++)
        dr_display[i] = *(volatile uint32_t *)(0xa05f8000 + i * 4);
}

static void display_restore(void) {
    int i;
    *(volatile uint32_t *)0xa05f8008 = 3;           /* nothing rendering any more */
    *(volatile uint32_t *)0xa05f8008 = 0;
    for(i = 0x040 / 4; i < 0x100 / 4; i++)
        *(volatile uint32_t *)(0xa05f8000 + i * 4) = dr_display[i];
}
#endif

int main(int argc, char **argv) {
#ifndef _arch_dreamcast
    if(argc > 1 && !strcmp(argv[1], "--labels"))
        return 0;                                   /* run2.sh asks every test for its labels: there are none */
    if(argc > 1)
        pvrio_folder = argv[1];
#else
    (void)argc; (void)argv;
#endif
    pvrio_open();
#ifdef _arch_dreamcast
    display_save();
#endif
    if(load()) {
#ifdef SETREGS
        {
            static const uint32_t set[] = SETREGS;
            unsigned k;
            for(k = 0; k + 1 < sizeof(set) / 4; k += 2) {
                pvrio_reg(set[k], set[k + 1]);
                regs[set[k] / 4] = set[k + 1];      /* the framebuffer dumped follows (FB_W_CTRL, FB_W_LINESTRIDE) */
            }
        }
#endif
#ifdef POKES
        {
            static const uint32_t pk[] = POKES;
            unsigned k;
            for(k = 0; k + 1 < sizeof(pk) / 4; k += 2)
                pvrio_w32(pk[k], &pk[k + 1], 1);
        }
#endif
#ifdef ZEXFIL
        dz_run();
#else
#ifdef TILES
        keep_tiles();
#endif
        {
            const uint32_t sof = regs[FB_W_SOF1_addr / 4] & 0xfffffcu, stride = (regs[FB_W_LINESTRIDE_addr / 4] & 0x1ff) * 8;
            const uint32_t lines = ((regs[FB_Y_CLIP_addr / 4] >> 16) & 0x3ff) + 1;
            pvrio_reg(STARTRENDER_addr, 0xffffffff);
            pvrio_wait(PVRIO_RENDER_DONE);
            pvrio_dump32(sof, (int)(stride * lines / 4), "frame", ".raw");
        }
#endif
    }
#ifdef _arch_dreamcast
    display_restore();
#endif
    pvrio_close();
    return 0;
}
