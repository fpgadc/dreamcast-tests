/* A dump of a game's frame (dreamster's polly2-data: pvr_regs_<scene>.bin, the 8 KB of 0x005F8000-0x005F9FFF,
   and vram_<scene>.bin, video memory in 32 bit area order) rendered by refsw2-offline and by a console, with the
   frame or a part of it: the same input on both sides, and the framebuffer dumped from both.

   Built by ../../dumprender.sh, which gives the defines:
     DUMPS     the folder of the dumps (on a console read through dc-load's /pc)
     SCENE     the scene's name
     TILES     optional: { x0, y0, x1, y1, ... } and NTILES: only the region array entries of these tiles
               (in their order in the array), the rest left out: a partial render
     DROP      optional: { t0, t1, ... }: the objects of these refsw2 tags (as its render log prints them: the
               parameter offset in words in bits 3-23) are left out of every list of every entry kept: their
               object list entries become strips with no triangle enabled
     NOLISTS   optional: a mask of the lists to leave out of every entry kept (1 opaque, 2 opaque modifier
               volumes, 4 translucent, 8 translucent modifier volumes, 16 punch through): their pointers are emptied
     SETREGS   optional: { offset, value, ... }: registers written after the dump's (e.g. SCALER_CTL 0x400, so that
               the framebuffer is the tiles as they were shaded, without the vertical filter)
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

#ifdef _arch_dreamcast
/* On a console the dump is part of the program: dc-load sends a program at about 13 MB/s, where reading the
   files through /pc takes minutes */
#define DR_STR2(x) #x
#define DR_STR(x) DR_STR2(x)
#define DR_P DR_STR(__USER_LABEL_PREFIX__)    /* sh-elf: C names have a leading underscore */
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
#ifdef _arch_dreamcast
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
#ifdef _arch_dreamcast
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

int main(int argc, char **argv) {
#ifndef _arch_dreamcast
    if(argc > 1)
        pvrio_folder = argv[1];
#else
    (void)argc; (void)argv;
#endif
    pvrio_open();
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
    }
    pvrio_close();
    return 0;
}
