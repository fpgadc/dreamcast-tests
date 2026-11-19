"""Game triangles for mipdump: tris.h from refsw3-offline render logs (TRIA lines: tag, ISP, TSP, TCW words and the
corners x, y, z, u, v in the order the rasterizer gets them).

    python3 gen.py <out tris.h> <log>:<tag>[,<tag>...] ...

Every float goes in as its bit pattern."""
import struct, sys
fb = lambda h: "0x%08xu" % struct.unpack("<I", struct.pack("<f", float.fromhex(h)))[0]
rows = []
for spec in sys.argv[2:]:
    log, tags = spec.rsplit(":", 1)
    lines = {l.split()[1]: l.split() for l in open(log, errors="replace") if l.startswith("TRIA: ")}
    for tag in tags.split(","):
        f = lines[tag]
        v = f[5:20]
        corners = ", ".join("{ " + ", ".join(fb(v[5 * k + i]) for i in range(5)) + " }" for k in range(3))
        rows.append(f'    {{ "{log.rsplit("/", 1)[-1].split(".")[0]} {tag}", 0x{f[3]}u, {{ {corners} }} }},')
open(sys.argv[1], "w").write(
    "/* made by gen.py from refsw3-offline render logs: the game's TSP word and the corners x, y, z, u, v (bit patterns,\n"
    "   in the rasterizer's order) */\n#include <stdint.h>\n"
    f"#define NUM_DTRIS {len(rows)}\n"
    "typedef struct { const char *name; uint32_t tsp; uint32_t v[3][5]; } dtri_t;\n"
    "static const dtri_t dtris[NUM_DTRIS] = {\n" + "\n".join(rows) + "\n};\n")
print(f"{len(rows)} triangles -> {sys.argv[1]}")
