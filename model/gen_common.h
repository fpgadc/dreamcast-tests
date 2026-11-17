/*
    What the test generators (pvr-tests/<test>/gen.cpp) share.

    A generator places its cases by what the hardware will do with them, and that knowledge exists once: in
    the soft floating point headers of refsw2 (nulldc3 libswirl/rend/refsw2: pvr_softfp.h, pvr_isp_edge.h,
    pvr_isp_depth.h, pvr_tsp_plane.h), the same ones the emulator and the probes of this folder compile. The
    functions under "the model" below only say which tile and which pixel; they add no arithmetic of their own.

    Also here: a small JSON reader / writer (the sets are archived as cases.json), a random number generator
    that gives the same numbers with every compiler, and the conventions of the tests (cells, bit patterns).

    Build: bash model/build.sh  ->  <test>/gen for every test that has a gen.cpp.
*/
#pragma once
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <unistd.h>
#include "pvr_softfp.h"
#include "pvr_isp_edge.h"
#include "pvr_isp_depth.h"
#include "pvr_tsp_plane.h"

namespace gen {

using pvrfp::Fp;

static const char Backslash = 92, Newline = 10;     // written as numbers: no escapes to get wrong on the way here

[[noreturn]] inline void Fail(const std::string& what)
{
    fprintf(stderr, "%s\n", what.c_str());
    exit(1);
}

// ---------------------------------------------------------------------------------------------------------
// single precision numbers as the tests hold them: bit patterns
// ---------------------------------------------------------------------------------------------------------
inline uint32_t Bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
inline float FromBits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
inline double F32(double v) { return (double)(float)v; }                 // the nearest single precision number
inline std::string Hex(double v)                                         // as it goes into cases.h
{
    char b[16];
    snprintf(b, sizeof b, "0x%08xu", (unsigned)Bits((float)v));
    return b;
}
// the single precision number k last-place steps from v (v is not zero)
inline double Step(double v, int k) { return (double)FromBits((uint32_t)((int32_t)Bits((float)v) + k)); }

// the screen is 20 x 15 cells of 32 x 32 pixels, one tile each
inline double CellX(int cell) { return (cell % 20) * 32.0; }
inline double CellY(int cell) { return (cell / 20) * 32.0; }

// ---------------------------------------------------------------------------------------------------------
// random numbers: xoshiro256**, seeded through splitmix64. Nothing from <random>: its distributions are not
// the same in every library, and a set has to come out again from its seed.
// ---------------------------------------------------------------------------------------------------------
struct Rng {
    uint64_t s[4];
    explicit Rng(uint64_t seed)
    {
        for (auto& v : s) {
            seed += 0x9e3779b97f4a7c15ull;
            uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            v = z ^ (z >> 31);
        }
    }
    static uint64_t Rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t next()
    {
        uint64_t r = Rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t; s[3] = Rotl(s[3], 45);
        return r;
    }
    double uniform() { return (double)(next() >> 11) * (1.0 / 9007199254740992.0); }      // [0, 1)
    double uniform(double a, double b) { return a + (b - a) * uniform(); }
    double logUniform(double a, double b) { return exp(uniform(log(a), log(b))); }
    int64_t integers(int64_t lo, int64_t hi)                                               // lo <= r < hi
    {
        uint64_t n = (uint64_t)(hi - lo), limit = UINT64_MAX - UINT64_MAX % n, r;
        do r = next(); while (r >= limit);
        return lo + (int64_t)(r % n);
    }
    bool chance(double p) { return uniform() < p; }
    int sign() { return integers(0, 2) ? 1 : -1; }
    size_t choice(const std::vector<double>& p)                                            // index, with these weights
    {
        double total = 0, u;
        for (double v : p) total += v;
        u = uniform() * total;
        for (size_t k = 0; k + 1 < p.size(); k++) {
            if (u < p[k]) return k;
            u -= p[k];
        }
        return p.size() - 1;
    }
    std::vector<int> permutation(int n)
    {
        std::vector<int> v(n);
        for (int k = 0; k < n; k++) v[k] = k;
        for (int k = n - 1; k > 0; k--) std::swap(v[k], v[integers(0, k + 1)]);
        return v;
    }
};

// ---------------------------------------------------------------------------------------------------------
// JSON: what cases.json needs. Numbers written without a fraction stay integers; objects keep their order.
// ---------------------------------------------------------------------------------------------------------
struct Json {
    enum Type { Null, Bool, Int, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    long long i = 0;
    double d = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    Json() {}
    Json(bool v) : type(Bool), b(v) {}
    Json(int v) : type(Int), i(v) {}
    Json(long v) : type(Int), i(v) {}
    Json(long long v) : type(Int), i(v) {}
    Json(size_t v) : type(Int), i((long long)v) {}
    Json(double v) : type(Num), d(v) {}
    Json(const char* v) : type(Str), s(v) {}
    Json(const std::string& v) : type(Str), s(v) {}
    static Json Array() { Json j; j.type = Arr; return j; }
    static Json Object() { Json j; j.type = Obj; return j; }
    template <class T> static Json List(const T* v, size_t n)
    {
        Json j = Array();
        for (size_t k = 0; k < n; k++) j.a.push_back(Json(v[k]));
        return j;
    }
    template <class T> static Json List(const std::vector<T>& v) { return List(v.data(), v.size()); }

    Json& push(Json v) { a.push_back(std::move(v)); return *this; }
    Json& set(const std::string& key, Json v)
    {
        for (auto& kv : o)
            if (kv.first == key) { kv.second = std::move(v); return *this; }
        o.emplace_back(key, std::move(v));
        return *this;
    }
    const Json* find(const std::string& key) const
    {
        for (auto& kv : o)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    bool has(const std::string& key) const { const Json* j = find(key); return j && j->type != Null; }
    const Json& at(const std::string& key) const
    {
        const Json* j = find(key);
        if (!j) Fail("cases.json: no \"" + key + "\"");
        return *j;
    }
    const Json& operator[](size_t k) const { return a[k]; }
    size_t size() const { return type == Arr ? a.size() : o.size(); }
    double num() const { return type == Int ? (double)i : type == Bool ? (double)b : d; }
    long long integer() const { return type == Int ? i : type == Bool ? (long long)b : (long long)d; }
    bool truthy() const { return type == Bool ? b : type == Int ? i != 0 : type == Num ? d != 0 : type != Null; }
    const std::string& str() const { return s; }
    std::vector<double> nums() const
    {
        std::vector<double> v;
        for (auto& e : a) v.push_back(e.num());
        return v;
    }

    void dump(std::string& out) const
    {
        char buf[40];
        switch (type) {
        case Null: out += "null"; break;
        case Bool: out += b ? "true" : "false"; break;
        case Int: snprintf(buf, sizeof buf, "%lld", i); out += buf; break;
        case Num:
            if (std::isnan(d)) out += "NaN";
            else if (std::isinf(d)) out += d < 0 ? "-Infinity" : "Infinity";
            else {
                snprintf(buf, sizeof buf, "%.17g", d);
                out += buf;
                if (!strpbrk(buf, ".e")) out += ".0";
            }
            break;
        case Str:
            out += '"';
            for (char c : s) {
                if (c == '"' || c == Backslash) { out += Backslash; out += c; }
                else if (c == Newline) { out += Backslash; out += 'n'; }
                else out += c;
            }
            out += '"';
            break;
        case Arr:
            out += '[';
            for (size_t k = 0; k < a.size(); k++) { if (k) out += ", "; a[k].dump(out); }
            out += ']';
            break;
        case Obj:
            out += '{';
            for (size_t k = 0; k < o.size(); k++) {
                if (k) out += ", ";
                Json(o[k].first).dump(out);
                out += ": ";
                o[k].second.dump(out);
            }
            out += '}';
            break;
        }
    }
    std::string dump() const { std::string out; dump(out); return out; }

    // ---- reading ----
    struct Reader {
        const char* p;
        const char* end;
        void ws() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) p++; }
        [[noreturn]] void bad(const char* what) { Fail(std::string("JSON: ") + what); }
        bool word(const char* w)
        {
            size_t n = strlen(w);
            if ((size_t)(end - p) >= n && !memcmp(p, w, n)) { p += n; return true; }
            return false;
        }
        std::string text()
        {
            std::string r;
            p++;
            while (p < end && *p != '"') {
                if (*p == Backslash && p + 1 < end) {
                    p++;
                    if (*p == 'n') r += '\n';
                    else if (*p == 't') r += '\t';
                    else if (*p == 'u') { r += (char)strtol(std::string(p + 1, 4).c_str(), nullptr, 16); p += 4; }
                    else r += *p;
                } else
                    r += *p;
                p++;
            }
            if (p >= end) bad("a string does not end");
            p++;
            return r;
        }
        Json value()
        {
            ws();
            if (p >= end) bad("nothing where a value should be");
            if (*p == '{') {
                Json j = Object();
                p++; ws();
                if (*p == '}') { p++; return j; }
                for (;;) {
                    ws();
                    if (*p != '"') bad("a key is not a string");
                    std::string k = text();
                    ws();
                    if (*p != ':') bad("no colon after a key");
                    p++;
                    j.o.emplace_back(k, value());
                    ws();
                    if (*p == ',') { p++; continue; }
                    if (*p == '}') { p++; return j; }
                    bad("an object does not end");
                }
            }
            if (*p == '[') {
                Json j = Array();
                p++; ws();
                if (*p == ']') { p++; return j; }
                for (;;) {
                    j.a.push_back(value());
                    ws();
                    if (*p == ',') { p++; continue; }
                    if (*p == ']') { p++; return j; }
                    bad("an array does not end");
                }
            }
            if (*p == '"') return Json(text());
            if (word("true")) return Json(true);
            if (word("false")) return Json(false);
            if (word("null")) return Json();
            if (word("NaN")) return Json(std::nan(""));
            if (word("Infinity")) return Json(HUGE_VAL);
            if (word("-Infinity")) return Json(-HUGE_VAL);
            const char* q = p;
            bool whole = true;
            while (q < end && (strchr("+-0123456789.eE", *q))) { if (strchr(".eE", *q)) whole = false; q++; }
            if (q == p) bad("not a value");
            std::string t(p, q);
            p = q;
            if (whole && t.size() < 18) return Json((long long)strtoll(t.c_str(), nullptr, 10));
            return Json(strtod(t.c_str(), nullptr));
        }
    };
    static Json Parse(const std::string& text)
    {
        Reader r{ text.data(), text.data() + text.size() };
        return r.value();
    }
};

inline std::string ReadFile(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) Fail("cannot read " + path);
    std::string s;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

inline void WriteFile(const std::string& path, const std::string& text)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f || fwrite(text.data(), 1, text.size(), f) != text.size()) Fail("cannot write " + path);
    fclose(f);
}

inline void WriteLines(const std::string& path, const std::vector<std::string>& lines)
{
    std::string s;
    for (auto& l : lines) { s += l; s += '\n'; }
    WriteFile(path, s);
}

// the folder of the generator itself: <test>/gen writes <test>/cases.h and cases.json
inline std::string Here()
{
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    std::string s(buf);
    return s.substr(0, s.find_last_of('/'));
}

inline std::string Format(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
inline std::string Format(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return buf;
}

// ---------------------------------------------------------------------------------------------------------
// the model: refsw2's headers, asked about one pixel. Coordinates are doubles that hold single precision
// values (what goes to the hardware); px, py is a pixel's sample point.
// ---------------------------------------------------------------------------------------------------------
struct Model {
    pvrisp::EdgeConfig edge;
    pvrisp::DepthConfig depth;
    pvrtsp::PlaneConfig plane;
};

// Where a sample point is: its tile's first pixel, and the pixel's column and row in the tile. centers: the
// ISP samples at pixel centers (HALF_OFFSET bit 0, the default), so the sample points are at x.5.
struct Pixel {
    int tx, ty, i, j;
    explicit Pixel(double px, double py, bool centers = true)
    {
        const double h = centers ? 0.5 : 0.0;
        const int X = (int)floor(px - h), Y = (int)floor(py - h);
        tx = (int)floor(X / 32.0) * 32; ty = (int)floor(Y / 32.0) * 32;
        i = X - tx; j = Y - ty;
    }
};

// The ISP's inside test of a polygon in one tile: 3 corners in rasterizer order, or 4 for a sprite (the
// winding is taken from the first three). rows: false for a modifier volume triangle, which has no row range.
struct Coverage {
    pvrisp::EdgeTile e[4];
    int n = 0, top = 0;
    bool bad = true;
    double first = -1e300, last = 1e300;

    void Setup(const Model& m, const double* x, const double* y, int corners, int tileLeft, int tileTop, bool centers = true, bool rows = true)
    {
        Fp fx[4], fy[4];
        float yf[4];
        n = corners; top = tileTop;
        first = -1e300; last = 1e300;
        for (int k = 0; k < n; k++) {
            fx[k] = Fp::FromFloat((float)x[k]); fy[k] = Fp::FromFloat((float)y[k]);
            yf[k] = (float)y[k];
        }
        const int sgn = pvrisp::Winding(fx, fy, m.edge, &bad);
        if (bad)
            return;
        if (rows)
            pvrisp::RowRange(yf, n, &first, &last);
        const int h = centers ? 1 : 0;
        for (int k = 0; k < n; k++)
            e[k].Setup(fx[k], fy[k], fx[(k + 1) % n], fy[(k + 1) % n], 2 * (int64_t)tileLeft + h, 2 * (int64_t)tileTop + h, sgn, m.edge);
    }
    bool Inside(int i, int j) const
    {
        if (bad || top + j < first || top + j > last)
            return false;
        for (int k = 0; k < n; k++)
            if (!e[k].Inside(i, j))
                return false;
        return true;
    }
};

inline bool Covered(const Model& m, const double* x, const double* y, int corners, double px, double py, bool centers = true, bool rows = true)
{
    Pixel p(px, py, centers);
    Coverage c;
    c.Setup(m, x, y, corners, p.tx, p.ty, centers, rows);
    return c.Inside(p.i, p.j);
}

// The depth the ISP compares and stores at a pixel (sampled at its center).
inline double IspDepth(const Model& m, const double* x, const double* y, const double* z, double px, double py)
{
    Pixel p(px, py);
    const float fx[3] = { (float)x[0], (float)x[1], (float)x[2] }, fy[3] = { (float)y[0], (float)y[1], (float)y[2] };
    const float fz[3] = { (float)z[0], (float)z[1], (float)z[2] };
    pvrisp::DepthTile d;
    d.Setup(fx, fy, fz, p.tx, p.ty, m.depth);
    return d.At(p.i, p.j, true);
}

// The column in its tile of the first pixel of the triangle on the pixel's line (0 when the triangle is not on
// that line of the tile): where the TSP starts its run when nothing hides the triangle.
inline int SpanStart(const Model& m, const double* x, const double* y, double px, double py)
{
    Pixel p(px, py);
    Coverage c;
    c.Setup(m, x, y, 3, p.tx, p.ty);
    for (int i = 0; i < 32; i++)
        if (c.Inside(i, p.j))
            return i;
    return 0;
}

// A TSP plane at a pixel: of z (1 / w at the corners), or of attr * z when attr is given. start: the column in
// the tile of the first pixel of the run the pixel belongs to (SpanStart, or the first visible pixel when a
// nearer polygon hides part of the line).
inline Fp TspPlane(const Model& m, const double* x, const double* y, const double* z, const double* attr, double px, double py, int start)
{
    Pixel p(px, py);
    const float fx[3] = { (float)x[0], (float)x[1], (float)x[2] }, fy[3] = { (float)y[0], (float)y[1], (float)y[2] };
    const float fz[3] = { (float)z[0], (float)z[1], (float)z[2] };
    float fa[3] = { 0, 0, 0 };
    if (attr)
        for (int k = 0; k < 3; k++) fa[k] = (float)attr[k];
    pvrtsp::Plane pl;
    pl.Setup(fx, fy, fz, attr ? fa : nullptr, m.plane);
    if (pl.bad) {
        Fp v;
        v.bad = true;
        return v;
    }
    return pl.Value(2 * (int64_t)(p.tx + start) + 1, 2 * (int64_t)(p.ty + p.j) + 1, p.i - start, p.tx, p.ty);
}

// (attribute / w) / (1 / w) as the TSP divides. A divisor that is not above zero is nothing a generator places
// cases on (refsw2 has its own rule for it in refsw_tile.h): plain division here.
inline double TspDivide(const Fp& num, const Fp& den)
{
    if (num.bad || den.bad)
        return std::nan("");
    const double d = den.toDouble(), n = num.toDouble();
    if (!(d > 0) || n == 0)
        return n / d;
    return pvrtsp::Divide(num, den).toDouble();
}

// An attribute (texture coordinate, color component) at the pixel, as a number.
inline double TspAttr(const Model& m, const double* x, const double* y, const double* z, const double* attr, double px, double py, int start)
{
    return TspDivide(TspPlane(m, x, y, z, attr, px, py, start), TspPlane(m, x, y, z, nullptr, px, py, start));
}

// A Gouraud color component (0 .. 255 at the corners) at the pixel: refsw2 adds 0.5 and cuts (COLOR_BIAS).
inline int TspColor(const Model& m, const double* x, const double* y, const double* z, const double* col, double px, double py, int start)
{
    double v = TspAttr(m, x, y, z, col, px, py, start);
    if (!(v == v))
        return 0;
    v = floor(v + 0.5);
    return v < 0 ? 0 : v > 255 ? 255 : (int)v;
}

} // namespace gen
