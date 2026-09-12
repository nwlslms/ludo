/* =========================================================
 *  LUDO 3D - SDL2 GUI/3D remake of ludo.c
 *
 *  - Software 3D renderer on top of SDL_Renderer
 *    (SDL_RenderGeometry, painter's algorithm, flat shading).
 *  - No external assets: board, pawns, dice and the 5x7 HUD
 *    font are generated procedurally.
 *  - Game rules are a faithful port of ludo.c.
 *
 *  Build : make ludo-gui
 *  Run   : ./build/ludo-gui
 *  Test  : ./build/ludo-gui --selftest [games]
 * ========================================================= */
#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#  else
#    include <SDL.h>
#  endif
#else
#  include <SDL.h>
#endif
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIN_W 1280
#define WIN_H 800
#define PI 3.14159265358979323846f
#define TAU (2.0f * PI)
#define MAX_TRIS 24000
#define PATH_MAX 32
#define VICTIM_MAX 8
#define ANIM_MAX 16
#define MSG_MAX 6

static double g_now = 0.0;
static float g_dt = 0.0f;

static void adv(double dt) { g_dt = (float)dt; g_now += dt; }

/* =========================================================
 *  Colors
 * ========================================================= */
static SDL_Color col(int r, int g, int b, int a) {
    SDL_Color c;
    c.r = (Uint8)(r < 0 ? 0 : (r > 255 ? 255 : r));
    c.g = (Uint8)(g < 0 ? 0 : (g > 255 ? 255 : g));
    c.b = (Uint8)(b < 0 ? 0 : (b > 255 ? 255 : b));
    c.a = (Uint8)(a < 0 ? 0 : (a > 255 ? 255 : a));
    return c;
}

static SDL_Color shade_color(SDL_Color c, float f) {
    return col((int)(c.r * f), (int)(c.g * f), (int)(c.b * f), c.a);
}

static SDL_Color team_col(int t) {
    switch (t) {
        case 0: return col(88, 196, 110, 255);
        case 1: return col(242, 204, 72, 255);
        case 2: return col(80, 140, 240, 255);
        default: return col(232, 88, 80, 255);
    }
}

static const char *TEAM_NAMES[4] = { "GREEN", "YELLOW", "BLUE", "RED" };

/* =========================================================
 *  3D math
 * ========================================================= */
typedef struct { float x, y, z; } Vec3;

static Vec3 v3(float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }
static Vec3 vadd(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 vsub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static Vec3 vmul(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float vdot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 vcross(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static Vec3 vnorm(Vec3 a) {
    float l = sqrtf(vdot(a, a));
    return l > 1e-6f ? vmul(a, 1.0f / l) : v3(0, 1, 0);
}
static Vec3 vlerp(Vec3 a, Vec3 b, float t) { return vadd(vmul(a, 1.0f - t), vmul(b, t)); }

typedef struct { float m[16]; } Mat4; /* row-major, m[row*4+col] */

static Mat4 mat_identity(void) {
    Mat4 m;
    memset(&m, 0, sizeof(m));
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

static Mat4 mat_mul(Mat4 a, Mat4 b) {
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    return r;
}

static Vec3 mat_apply(Mat4 m, Vec3 p) {
    return v3(m.m[0] * p.x + m.m[1] * p.y + m.m[2] * p.z + m.m[3],
              m.m[4] * p.x + m.m[5] * p.y + m.m[6] * p.z + m.m[7],
              m.m[8] * p.x + m.m[9] * p.y + m.m[10] * p.z + m.m[11]);
}

static Vec3 mat_apply4(Mat4 m, Vec3 p, float *w) {
    Vec3 r;
    r.x = m.m[0] * p.x + m.m[1] * p.y + m.m[2] * p.z + m.m[3];
    r.y = m.m[4] * p.x + m.m[5] * p.y + m.m[6] * p.z + m.m[7];
    r.z = m.m[8] * p.x + m.m[9] * p.y + m.m[10] * p.z + m.m[11];
    *w = m.m[12] * p.x + m.m[13] * p.y + m.m[14] * p.z + m.m[15];
    return r;
}

static Mat4 mat_translate(float x, float y, float z) {
    Mat4 m = mat_identity();
    m.m[3] = x; m.m[7] = y; m.m[11] = z;
    return m;
}

static Mat4 mat_scale(float x, float y, float z) {
    Mat4 m = mat_identity();
    m.m[0] = x; m.m[5] = y; m.m[10] = z;
    return m;
}

static Mat4 mat_rot_x(float a) {
    Mat4 m = mat_identity();
    float c = cosf(a), s = sinf(a);
    m.m[5] = c; m.m[6] = -s; m.m[9] = s; m.m[10] = c;
    return m;
}

static Mat4 mat_rot_y(float a) {
    Mat4 m = mat_identity();
    float c = cosf(a), s = sinf(a);
    m.m[0] = c; m.m[2] = s; m.m[8] = -s; m.m[10] = c;
    return m;
}

static Mat4 mat_rot_z(float a) {
    Mat4 m = mat_identity();
    float c = cosf(a), s = sinf(a);
    m.m[0] = c; m.m[1] = -s; m.m[4] = s; m.m[5] = c;
    return m;
}

static Mat4 mat_perspective(float fovy, float aspect, float zn, float zf) {
    Mat4 m;
    memset(&m, 0, sizeof(m));
    float f = 1.0f / tanf(fovy * 0.5f);
    m.m[0] = f / aspect;
    m.m[5] = f;
    m.m[10] = (zf + zn) / (zn - zf);
    m.m[11] = 2.0f * zf * zn / (zn - zf);
    m.m[14] = -1.0f;
    return m;
}

static Mat4 mat_lookat(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vnorm(vsub(center, eye));
    Vec3 s = vnorm(vcross(f, up));
    Vec3 u = vcross(s, f);
    Mat4 m = mat_identity();
    m.m[0] = s.x;  m.m[1] = s.y;  m.m[2] = s.z;  m.m[3] = -vdot(s, eye);
    m.m[4] = u.x;  m.m[5] = u.y;  m.m[6] = u.z;  m.m[7] = -vdot(u, eye);
    m.m[8] = -f.x; m.m[9] = -f.y; m.m[10] = -f.z; m.m[11] = vdot(f, eye);
    return m;
}

/* =========================================================
 *  Built-in 5x7 bitmap font
 * ========================================================= */
typedef struct { char c; const char *art; } GlyphDef;

#define G(ch, s) { ch, s }
static const GlyphDef GLYPHS[] = {
    G(' ',  "...../...../...../...../...../...../....."),
    G('A',  ".###./#...#/#...#/#####/#...#/#...#/#...#"),
    G('B',  "####./#...#/#...#/####./#...#/#...#/####."),
    G('C',  ".###./#...#/#..../#..../#..../#...#/.###."),
    G('D',  "####./#...#/#...#/#...#/#...#/#...#/####."),
    G('E',  "#####/#..../#..../####./#..../#..../#####"),
    G('F',  "#####/#..../#..../####./#..../#..../#...."),
    G('G',  ".###./#...#/#..../#.###/#...#/#...#/.###."),
    G('H',  "#...#/#...#/#...#/#####/#...#/#...#/#...#"),
    G('I',  "#####/..#../..#../..#../..#../..#../#####"),
    G('J',  "..###/...#./...#./...#./...#./#..#./.##.."),
    G('K',  "#...#/#..#./#.#../##.../#.#../#..#./#...#"),
    G('L',  "#..../#..../#..../#..../#..../#..../#####"),
    G('M',  "#...#/##.##/#.#.#/#.#.#/#...#/#...#/#...#"),
    G('N',  "#...#/##..#/#.#.#/#..##/#...#/#...#/#...#"),
    G('O',  ".###./#...#/#...#/#...#/#...#/#...#/.###."),
    G('P',  "####./#...#/#...#/####./#..../#..../#...."),
    G('Q',  ".###./#...#/#...#/#...#/#.#.#/#..#./.##.#"),
    G('R',  "####./#...#/#...#/####./#.#../#..#./#...#"),
    G('S',  ".####/#..../#..../.###./....#/....#/####."),
    G('T',  "#####/..#../..#../..#../..#../..#../..#.."),
    G('U',  "#...#/#...#/#...#/#...#/#...#/#...#/.###."),
    G('V',  "#...#/#...#/#...#/#...#/#...#/.#.#./..#.."),
    G('W',  "#...#/#...#/#...#/#.#.#/#.#.#/##.##/#...#"),
    G('X',  "#...#/#...#/.#.#./..#../.#.#./#...#/#...#"),
    G('Y',  "#...#/#...#/.#.#./..#../..#../..#../..#.."),
    G('Z',  "#####/....#/...#./..#../.#.../#..../#####"),
    G('0',  ".###./#...#/#..##/#.#.#/##..#/#...#/.###."),
    G('1',  "..#../.##../..#../..#../..#../..#../.###."),
    G('2',  ".###./#...#/....#/...#./..#../.#.../#####"),
    G('3',  "####./....#/....#/.###./....#/....#/####."),
    G('4',  "#...#/#...#/#...#/#####/....#/....#/....#"),
    G('5',  "#####/#..../#..../####./....#/....#/####."),
    G('6',  ".###./#..../#..../####./#...#/#...#/.###."),
    G('7',  "#####/....#/...#./..#../.#.../.#.../.#..."),
    G('8',  ".###./#...#/#...#/.###./#...#/#...#/.###."),
    G('9',  ".###./#...#/#...#/.####/....#/....#/.###."),
    G('.',  "...../...../...../...../...../.##../.##.."),
    G(',',  "...../...../...../...../.##../.##../.#..."),
    G('!',  "..#../..#../..#../..#../..#../...../..#.."),
    G('?',  ".###./#...#/....#/...#./..#../...../..#.."),
    G(':',  "...../.##../.##../...../.##../.##../....."),
    G(';',  "...../.##../.##../...../.##../.##../.#..."),
    G('\'', "..#../..#../...../...../...../...../....."),
    G('"',  ".#.#./.#.#./...../...../...../...../....."),
    G('-',  "...../...../...../#####/...../...../....."),
    G('+',  "...../..#../..#../#####/..#../..#../....."),
    G('=',  "...../...../#####/...../#####/...../....."),
    G('/',  "....#/....#/...#./..#../.#.../#..../#...."),
    G('\\', "#..../#..../.#.../..#../...#./....#/....#"),
    G('(',  "..##./.#.../.#.../.#.../.#.../.#.../..##."),
    G(')',  ".##../...#./...#./...#./...#./...#./.##.."),
    G('<',  "...#./..#../.#.../#..../.#.../..#../...#."),
    G('>',  ".#.../..#../...#./....#/...#./..#../.#..."),
    G('*',  "...../#.#.#/.###./#####/.###./#.#.#/....."),
    G('#',  ".#.#./.#.#./#####/.#.#./#####/.#.#./.#.#."),
    G('%',  "##..#/##.#./..#../.#.../#.##./.#.##/....."),
    G('_',  "...../...../...../...../...../...../#####"),
    G('[',  "..##./..#../..#../..#../..#../..#../..##."),
    G(']',  ".##../..#../..#../..#../..#../..#../.##.."),
    G('|',  "..#../..#../..#../..#../..#../..#../..#.."),
};

static unsigned char g_font[128][7];

static void font_init(void) {
    memset(g_font, 0, sizeof(g_font));
    for (size_t i = 0; i < sizeof(GLYPHS) / sizeof(GLYPHS[0]); i++) {
        char c = GLYPHS[i].c;
        const char *art = GLYPHS[i].art;
        for (int row = 0; row < 7; row++) {
            unsigned char bits = 0;
            for (int c2 = 0; c2 < 5; c2++)
                if (art[row * 6 + c2] == '#') bits |= (unsigned char)(1 << (4 - c2));
            g_font[(unsigned char)c][row] = bits;
        }
    }
}

static float text_w(const char *t, float s) { return (float)strlen(t) * 6.0f * s - s; }

static void draw_text(SDL_Renderer *r, float x, float y, float s, SDL_Color c, const char *t) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (; *t; t++) {
        unsigned char ch = (unsigned char)*t;
        if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 32);
        if (ch >= 128) ch = '?';
        for (int row = 0; row < 7; row++) {
            unsigned char bits = g_font[ch][row];
            if (!bits) continue;
            for (int c2 = 0; c2 < 5; c2++)
                if (bits & (1 << (4 - c2))) {
                    SDL_Rect rc;
                    rc.x = (int)(x + c2 * s);
                    rc.y = (int)(y + row * s);
                    rc.w = (int)(s + 0.5f);
                    rc.h = (int)(s + 0.5f);
                    SDL_RenderFillRect(r, &rc);
                }
        }
        x += 6.0f * s;
    }
}

static void draw_text_center(SDL_Renderer *r, float cx, float y, float s, SDL_Color c, const char *t) {
    draw_text(r, cx - text_w(t, s) * 0.5f, y, s, c, t);
}

/* =========================================================
 *  Mesh primitives
 * ========================================================= */
typedef struct { Vec3 p[3]; SDL_Color c; } Tri;
typedef struct { Tri *t; int n, cap; } Mesh;

static void mesh_init(Mesh *m) { m->t = NULL; m->n = 0; m->cap = 0; }

static void mesh_push(Mesh *m, Vec3 a, Vec3 b, Vec3 c, SDL_Color colr) {
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 256;
        m->t = (Tri *)realloc(m->t, (size_t)m->cap * sizeof(Tri));
        if (!m->t) { fprintf(stderr, "out of memory\n"); exit(1); }
    }
    m->t[m->n].p[0] = a;
    m->t[m->n].p[1] = b;
    m->t[m->n].p[2] = c;
    m->t[m->n].c = colr;
    m->n++;
}

static void add_tri(Mesh *m, Vec3 a, Vec3 b, Vec3 c, SDL_Color colr) { mesh_push(m, a, b, c, colr); }
static void add_quad(Mesh *m, Vec3 a, Vec3 b, Vec3 c, Vec3 d, SDL_Color colr) {
    mesh_push(m, a, b, c, colr);
    mesh_push(m, a, c, d, colr);
}

static void add_box(Mesh *m, float x0, float y0, float z0, float x1, float y1, float z1,
                    SDL_Color top, SDL_Color side, SDL_Color bottom) {
    Vec3 a = v3(x0, y1, z0), b = v3(x1, y1, z0), c = v3(x1, y1, z1), d = v3(x0, y1, z1);
    Vec3 e = v3(x0, y0, z0), f = v3(x1, y0, z0), g = v3(x1, y0, z1), h = v3(x0, y0, z1);
    add_quad(m, a, b, c, d, top);
    add_quad(m, h, g, f, e, bottom);
    add_quad(m, a, d, h, e, side);
    add_quad(m, b, c, g, f, side);
    add_quad(m, d, c, g, h, side);
    add_quad(m, a, b, f, e, side);
}

/* box without top/bottom faces - only the 4 side wall quads */
static void add_box_sides(Mesh *m, float x0, float y0, float z0, float x1, float y1, float z1,
                          SDL_Color side) {
    Vec3 a = v3(x0, y1, z0), b = v3(x1, y1, z0), c = v3(x1, y1, z1), d = v3(x0, y1, z1);
    Vec3 e = v3(x0, y0, z0), f = v3(x1, y0, z0), g = v3(x1, y0, z1), h = v3(x0, y0, z1);
    add_quad(m, a, d, h, e, side);
    add_quad(m, b, c, g, f, side);
    add_quad(m, d, c, g, h, side);
    add_quad(m, a, b, f, e, side);
}

static void add_tile(Mesh *m, float cx, float cz, float half, float ytop, float ybot, SDL_Color colr) {
    float iv = 0.07f;
    SDL_Color side = shade_color(colr, 0.72f);
    Vec3 t0 = v3(cx - half + iv, ytop, cz - half + iv);
    Vec3 t1 = v3(cx + half - iv, ytop, cz - half + iv);
    Vec3 t2 = v3(cx + half - iv, ytop, cz + half - iv);
    Vec3 t3 = v3(cx - half + iv, ytop, cz + half - iv);
    add_quad(m, t0, t1, t2, t3, colr);
    Vec3 b0 = v3(cx - half, ybot, cz - half);
    Vec3 b1 = v3(cx + half, ybot, cz - half);
    Vec3 b2 = v3(cx + half, ybot, cz + half);
    Vec3 b3 = v3(cx - half, ybot, cz + half);
    add_quad(m, t0, t1, b1, b0, side);
    add_quad(m, t1, t2, b2, b1, side);
    add_quad(m, t2, t3, b3, b2, side);
    add_quad(m, t3, t0, b0, b3, side);
}

static void add_disc(Mesh *m, float cx, float y, float cz, float r, SDL_Color colr, int segs) {
    Vec3 c = v3(cx, y, cz);
    for (int i = 0; i < segs; i++) {
        float a0 = TAU * i / segs, a1 = TAU * (i + 1) / segs;
        add_tri(m, c,
                v3(cx + cosf(a0) * r, y, cz + sinf(a0) * r),
                v3(cx + cosf(a1) * r, y, cz + sinf(a1) * r), colr);
    }
}

static void add_disc3(Mesh *m, Vec3 c, Vec3 u, Vec3 v, float r, SDL_Color colr, int segs) {
    for (int i = 0; i < segs; i++) {
        float a0 = TAU * i / segs, a1 = TAU * (i + 1) / segs;
        Vec3 p0 = vadd(c, vadd(vmul(u, cosf(a0) * r), vmul(v, sinf(a0) * r)));
        Vec3 p1 = vadd(c, vadd(vmul(u, cosf(a1) * r), vmul(v, sinf(a1) * r)));
        add_tri(m, c, p0, p1, colr);
    }
}

static void add_cyl(Mesh *m, float cx, float y0, float y1, float r0, float r1, float cz,
                    SDL_Color colr, int segs) {
    for (int i = 0; i < segs; i++) {
        float a0 = TAU * i / segs, a1 = TAU * (i + 1) / segs;
        Vec3 p00 = v3(cx + cosf(a0) * r0, y0, cz + sinf(a0) * r0);
        Vec3 p10 = v3(cx + cosf(a1) * r0, y0, cz + sinf(a1) * r0);
        Vec3 p11 = v3(cx + cosf(a1) * r1, y1, cz + sinf(a1) * r1);
        Vec3 p01 = v3(cx + cosf(a0) * r1, y1, cz + sinf(a0) * r1);
        add_quad(m, p00, p10, p11, p01, colr);
    }
    add_disc(m, cx, y1, cz, r1, shade_color(colr, 1.05f), segs);
}

static void add_sphere(Mesh *m, float cx, float cy, float cz, float r, SDL_Color colr,
                       int segs, int rings) {
    for (int i = 0; i < rings; i++) {
        float p0 = PI * i / rings, p1 = PI * (i + 1) / rings;
        for (int s = 0; s < segs; s++) {
            float t0 = TAU * s / segs, t1 = TAU * (s + 1) / segs;
            Vec3 a = v3(cx + r * sinf(p0) * cosf(t0), cy + r * cosf(p0), cz + r * sinf(p0) * sinf(t0));
            Vec3 b = v3(cx + r * sinf(p0) * cosf(t1), cy + r * cosf(p0), cz + r * sinf(p0) * sinf(t1));
            Vec3 c = v3(cx + r * sinf(p1) * cosf(t1), cy + r * cosf(p1), cz + r * sinf(p1) * sinf(t1));
            Vec3 d = v3(cx + r * sinf(p1) * cosf(t0), cy + r * cosf(p1), cz + r * sinf(p1) * sinf(t0));
            add_quad(m, a, b, c, d, colr);
        }
    }
}

/* =========================================================
 *  Software 3D renderer
 * ========================================================= */
typedef struct { SDL_Vertex v[3]; float depth; } SortTri;

static SortTri g_st[MAX_TRIS];
static SDL_Vertex g_out[MAX_TRIS * 3];
static int g_stn = 0;
static Mat4 g_view, g_proj;
static Vec3 g_light;

static float camYaw = -0.65f, camPitch = 0.82f, camDist = 24.5f;
static Vec3 camTarget;

static void build_view_proj(void) {
    float cp = cosf(camPitch), sp = sinf(camPitch);
    Vec3 eye = v3(camTarget.x + camDist * cp * sinf(camYaw),
                  camTarget.y + camDist * sp,
                  camTarget.z + camDist * cp * cosf(camYaw));
    g_view = mat_lookat(eye, camTarget, v3(0, 1, 0));
    g_proj = mat_perspective(54.0f * PI / 180.0f, (float)WIN_W / (float)WIN_H, 0.2f, 200.0f);
}

static void emit_tri(Vec3 a, Vec3 b, Vec3 c, SDL_Color baseCol, SDL_Color tint) {
    if (g_stn >= MAX_TRIS) return;
    Vec3 n = vnorm(vcross(vsub(b, a), vsub(c, a)));
    float lam = fabsf(vdot(n, g_light));
    float sh = 0.45f + 0.55f * lam;
    int r = (int)(baseCol.r * tint.r / 255.0f * sh + 6.0f);
    int g = (int)(baseCol.g * tint.g / 255.0f * sh + 6.0f);
    int bl = (int)(baseCol.b * tint.b / 255.0f * sh + 6.0f);
    SDL_Color vc = col(r, g, bl, baseCol.a < tint.a ? baseCol.a : tint.a);

    Vec3 ea = mat_apply(g_view, a), eb = mat_apply(g_view, b), ec = mat_apply(g_view, c);
    if (ea.z > -0.1f || eb.z > -0.1f || ec.z > -0.1f) return;

    float wa, wb, wc;
    Vec3 ca = mat_apply4(g_proj, ea, &wa);
    Vec3 cb = mat_apply4(g_proj, eb, &wb);
    Vec3 cc = mat_apply4(g_proj, ec, &wc);
    if (wa < 1e-4f || wb < 1e-4f || wc < 1e-4f) return;

    SortTri *t = &g_st[g_stn++];
    SDL_Vertex *v = t->v;
    v[0].position.x = (ca.x / wa * 0.5f + 0.5f) * WIN_W;
    v[0].position.y = (1.0f - (ca.y / wa * 0.5f + 0.5f)) * WIN_H;
    v[1].position.x = (cb.x / wb * 0.5f + 0.5f) * WIN_W;
    v[1].position.y = (1.0f - (cb.y / wb * 0.5f + 0.5f)) * WIN_H;
    v[2].position.x = (cc.x / wc * 0.5f + 0.5f) * WIN_W;
    v[2].position.y = (1.0f - (cc.y / wc * 0.5f + 0.5f)) * WIN_H;
    for (int i = 0; i < 3; i++) {
        v[i].color = vc;
        v[i].tex_coord.x = 0;
        v[i].tex_coord.y = 0;
    }
    t->depth = (ea.z + eb.z + ec.z) / 3.0f;
}

static void render_mesh(const Mesh *m, Mat4 model, SDL_Color tint) {
    for (int i = 0; i < m->n; i++) {
        Vec3 a = mat_apply(model, m->t[i].p[0]);
        Vec3 b = mat_apply(model, m->t[i].p[1]);
        Vec3 c = mat_apply(model, m->t[i].p[2]);
        emit_tri(a, b, c, m->t[i].c, tint);
    }
}

static int cmp_sorttri(const void *pa, const void *pb) {
    const SortTri *a = (const SortTri *)pa;
    const SortTri *b = (const SortTri *)pb;
    if (a->depth < b->depth) return -1;
    if (a->depth > b->depth) return 1;
    return 0;
}

static void flush3d(SDL_Renderer *r) {
    qsort(g_st, (size_t)g_stn, sizeof(SortTri), cmp_sorttri);
    for (int i = 0; i < g_stn; i++) {
        g_out[i * 3 + 0] = g_st[i].v[0];
        g_out[i * 3 + 1] = g_st[i].v[1];
        g_out[i * 3 + 2] = g_st[i].v[2];
    }
    if (g_stn) SDL_RenderGeometry(r, NULL, g_out, g_stn * 3, NULL, 0);
    g_stn = 0;
}

static int project_point(Vec3 w, float *sx, float *sy) {
    Vec3 e = mat_apply(g_view, w);
    if (e.z > -0.1f) return 0;
    float wc;
    Vec3 c = mat_apply4(g_proj, e, &wc);
    if (wc < 1e-4f) return 0;
    *sx = (c.x / wc * 0.5f + 0.5f) * WIN_W;
    *sy = (1.0f - (c.y / wc * 0.5f + 0.5f)) * WIN_H;
    return 1;
}

/* =========================================================
 *  Game data (ported from ludo.c)
 * ========================================================= */
typedef struct {
    short masuk;
    short kebal;
    short tamat;
    short y, x;
} Bidak;

typedef struct {
    short warna;
    short inside[4];
    short insd;
    short outside[4];
    short otsd;
    short selesai;
} Team;

static Bidak bidak[4][4];
static char map[15][16] = {"111111000222222",
                           "100001022200002",
                           "100001920200002",
                           "100001020200002",
                           "100001020200002",
                           "111111020222222",
                           "010000122000900",
                           "011111193333330",
                           "009000443000030",
                           "444444040333333",
                           "400004040300003",
                           "400004040300003",
                           "400004049300003",
                           "400004440300003",
                           "444444000333333"};

static Team green, yellow, blue, red;
static Team *teams[4] = { &green, &yellow, &blue, &red };

/* [team][pawn] = { y, x } -- home yard spots */
static const short BASE_POS[4][4][2] = {
    { {1, 2}, {1, 3}, {3, 2}, {3, 3} },
    { {1, 11}, {1, 12}, {3, 11}, {3, 12} },
    { {11, 11}, {11, 12}, {13, 11}, {13, 12} },
    { {11, 2}, {11, 3}, {13, 2}, {13, 3} }
};

static float g_pawnYaw[4][4];

static int yard_of(int gy, int gx) {
    if (gy < 6 && gx < 6) return 0;
    if (gy < 6 && gx > 8) return 1;
    if (gy > 8 && gx > 8) return 2;
    if (gy > 8 && gx < 6) return 3;
    return -1;
}

static int yard_interior(int gy, int gx) {
    int y = yard_of(gy, gx);
    if (y < 0) return 0;
    int r = (y == 0 || y == 1) ? gy : gy - 9;
    int c = (y == 0 || y == 3) ? gx : gx - 9;
    return (r >= 1 && r <= 4 && c >= 1 && c <= 4);
}

static float cell_top(int gy, int gx) {
    int y = yard_of(gy, gx);
    if (y >= 0) return yard_interior(gy, gx) ? 0.16f : 0.30f;
    return 0.10f;
}

static Vec3 cell_world(int gx, int gy) {
    return v3((float)gx - 7.0f, cell_top(gy, gx), (float)gy - 7.0f);
}

static SDL_Color tile_color(char c) {
    switch (c) {
        case '1': return col(64, 168, 86, 255);
        case '2': return col(226, 190, 58, 255);
        case '3': return col(64, 112, 208, 255);
        case '4': return col(206, 66, 62, 255);
        case '9': return col(120, 122, 134, 255);
        default:  return col(226, 222, 212, 255);
    }
}

/* =========================================================
 *  Static meshes: board, pawn, die
 * ========================================================= */
static Mesh g_board, g_pawn, g_die, g_shadow;

static void build_shadow(void) {
    mesh_init(&g_shadow);
    add_disc(&g_shadow, 0, 0, 0, 1.0f, col(255, 255, 255, 255), 18);
}

static void build_board(void) {
    mesh_init(&g_board);
    SDL_Color wood_top = col(94, 70, 52, 255);
    SDL_Color wood_side = col(70, 52, 40, 255);
    SDL_Color wood_bot = col(44, 32, 26, 255);

    add_box_sides(&g_board, -7.9f, -0.9f, -7.9f, 7.9f, 0.0f, 7.9f, wood_side);
    add_box(&g_board, -7.9f, 0.0f, -7.9f, 7.9f, 0.18f, -7.55f, wood_top, wood_side, wood_bot);
    add_box(&g_board, -7.9f, 0.0f, 7.55f, 7.9f, 0.18f, 7.9f, wood_top, wood_side, wood_bot);
    add_box(&g_board, -7.9f, 0.0f, -7.55f, -7.55f, 0.18f, 7.55f, wood_top, wood_side, wood_bot);
    add_box(&g_board, 7.55f, 0.0f, -7.55f, 7.9f, 0.18f, 7.55f, wood_top, wood_side, wood_bot);

    for (int gy = 0; gy < 15; gy++)
        for (int gx = 0; gx < 15; gx++) {
            SDL_Color tc = tile_color(map[gy][gx]);
            unsigned h = (unsigned)(gx * 73856093u) ^ (unsigned)(gy * 19349663u);
            float j = 0.94f + 0.12f * ((h >> 8) % 1000) / 1000.0f;
            tc = shade_color(tc, j);
            add_tile(&g_board, (float)gx - 7.0f, (float)gy - 7.0f, 0.49f,
                     cell_top(gy, gx), 0.0f, tc);
        }

    /* star markers on safe tiles ('9'), skipping the center */
    for (int gy = 0; gy < 15; gy++)
        for (int gx = 0; gx < 15; gx++) {
            if (map[gy][gx] != '9' || (gx == 7 && gy == 7)) continue;
            float cx = (float)gx - 7.0f, cz = (float)gy - 7.0f;
            float y = cell_top(gy, gx) + 0.016f;
            SDL_Color sc = col(238, 238, 244, 255);
            const float r1 = 0.30f, r2 = 0.12f;
            Vec3 c = v3(cx, y, cz);
            Vec3 tips[4] = {v3(cx, y, cz - r1), v3(cx + r1, y, cz),
                            v3(cx, y, cz + r1), v3(cx - r1, y, cz)};
            Vec3 nots[4] = {v3(cx + r2, y, cz - r2), v3(cx + r2, y, cz + r2),
                            v3(cx - r2, y, cz + r2), v3(cx - r2, y, cz - r2)};
            for (int i = 0; i < 4; i++) {
                add_tri(&g_board, c, nots[i], tips[i], sc);
                add_tri(&g_board, c, tips[i], nots[(i + 1) % 4], sc);
            }
        }

    /* home yard pawn spots */
    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 4; i++) {
            float wx = (float)BASE_POS[t][i][1] - 7.0f;
            float wz = (float)BASE_POS[t][i][0] - 7.0f;
            add_disc(&g_board, wx, 0.175f, wz, 0.34f, shade_color(team_col(t), 0.55f), 18);
            add_disc(&g_board, wx, 0.19f, wz, 0.26f, shade_color(team_col(t), 0.80f), 18);
        }

    /* center marker: small pinwheel inside the middle tile */
    float cy = 0.135f;
    Vec3 ctr = v3(0, cy, 0);
    add_tri(&g_board, ctr, v3(-0.48f, cy, -0.48f), v3(-0.48f, cy, 0.48f), team_col(0));
    add_tri(&g_board, ctr, v3(-0.48f, cy, -0.48f), v3(0.48f, cy, -0.48f), team_col(1));
    add_tri(&g_board, ctr, v3(0.48f, cy, -0.48f), v3(0.48f, cy, 0.48f), team_col(2));
    add_tri(&g_board, ctr, v3(-0.48f, cy, 0.48f), v3(0.48f, cy, 0.48f), team_col(3));
    add_disc(&g_board, 0, cy + 0.01f, 0, 0.16f, col(30, 30, 38, 255), 14);
}

static void build_pawn(void) {
    SDL_Color w = col(255, 255, 255, 255);
    mesh_init(&g_pawn);
    add_disc(&g_pawn, 0, 0.015f, 0, 0.30f, w, 12);
    add_cyl(&g_pawn, 0, 0.0f, 0.10f, 0.28f, 0.22f, 0, w, 12);
    add_cyl(&g_pawn, 0, 0.10f, 0.40f, 0.21f, 0.11f, 0, w, 12);
    add_cyl(&g_pawn, 0, 0.38f, 0.46f, 0.13f, 0.13f, 0, w, 12);
    add_sphere(&g_pawn, 0, 0.58f, 0, 0.165f, w, 12, 6);
}

static void build_die(void) {
    SDL_Color body = col(242, 240, 232, 255);
    SDL_Color pip = col(40, 38, 48, 255);
    float h = 0.48f;
    mesh_init(&g_die);

    struct { int axis; float sign; Vec3 u, v; int val; } faces[6] = {
        {1,  1.0f, {1, 0, 0},  {0, 0, 1},  1},
        {1, -1.0f, {1, 0, 0},  {0, 0, -1}, 6},
        {2,  1.0f, {1, 0, 0},  {0, 1, 0},  2},
        {2, -1.0f, {-1, 0, 0}, {0, 1, 0},  5},
        {0,  1.0f, {0, 0, -1}, {0, 1, 0},  3},
        {0, -1.0f, {0, 0, 1},  {0, 1, 0},  4},
    };
    static const float pipPos[6][6][2] = {
        { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
        { {-1, -1}, {1, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
        { {-1, -1}, {0, 0}, {1, 1}, {0, 0}, {0, 0}, {0, 0} },
        { {-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {0, 0}, {0, 0} },
        { {-1, -1}, {-1, 1}, {0, 0}, {1, -1}, {1, 1}, {0, 0} },
        { {-1, -1}, {-1, 0}, {-1, 1}, {1, -1}, {1, 0}, {1, 1} },
    };
    static const int pipN[6] = {1, 2, 3, 4, 5, 6};

    for (int f = 0; f < 6; f++) {
        Vec3 n = (faces[f].axis == 0) ? v3(faces[f].sign, 0, 0)
               : (faces[f].axis == 1) ? v3(0, faces[f].sign, 0)
                                      : v3(0, 0, faces[f].sign);
        Vec3 u = faces[f].u, v = faces[f].v;
        Vec3 c = vmul(n, h);
        add_quad(&g_die,
                 vadd(c, vadd(vmul(u, -h), vmul(v, -h))),
                 vadd(c, vadd(vmul(u,  h), vmul(v, -h))),
                 vadd(c, vadd(vmul(u,  h), vmul(v,  h))),
                 vadd(c, vadd(vmul(u, -h), vmul(v,  h))), body);
        int val = faces[f].val;
        int np = pipN[val - 1];
        for (int p = 0; p < np; p++) {
            float pu = pipPos[val - 1][p][0] * h * 0.58f;
            float pv = pipPos[val - 1][p][1] * h * 0.58f;
            Vec3 pc = vadd(vadd(c, vmul(n, h * 0.012f)), vadd(vmul(u, pu), vmul(v, pv)));
            add_disc3(&g_die, pc, u, v, h * 0.16f, pip, 10);
        }
    }
}

/* =========================================================
 *  Match log
 * ========================================================= */
static char g_log[MSG_MAX][96];
static SDL_Color g_logCol[MSG_MAX];
static int g_logCount = 0;
static char g_banner[96];
static SDL_Color g_bannerCol;
static double g_bannerT = -10.0;

static void msg(SDL_Color c, const char *fmt, ...) {
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    int idx = g_logCount % MSG_MAX;
    snprintf(g_log[idx], sizeof(g_log[idx]), "%s", buf);
    g_logCol[idx] = c;
    g_logCount++;
    snprintf(g_banner, sizeof(g_banner), "%s", buf);
    g_bannerCol = c;
    g_bannerT = g_now;
}

/* =========================================================
 *  Game logic (faithful port of ludo.c)
 * ========================================================= */
static short g_path[PATH_MAX][2]; /* [k][0]=x, [k][1]=y */
static int g_pathLen = 0;
static int g_pathRec = 0;

static short dadu(void) { return (short)((rand() % 6) + 1); }

static Team set(short warna) {
    Team temp;
    temp.warna = warna;
    temp.inside[0] = 1;
    temp.inside[1] = 2;
    temp.inside[2] = 3;
    temp.inside[3] = 4;
    temp.insd = 4;
    temp.otsd = 0;
    temp.selesai = 0;
    memset(temp.outside, 0, sizeof(temp.outside));
    return temp;
}

static void langkahSatu(Team *p, short pilihan) {
    Bidak *b = &bidak[p->warna - 1][pilihan - 1];
    switch (p->warna) {
        case 1:
            if (b->x == 0 && b->y == 8) b->masuk = 1;
            break;
        case 2:
            if (b->x == 6 && b->y == 0) b->masuk = 1;
            break;
        case 3:
            if (b->x == 14 && b->y == 6) b->masuk = 1;
            break;
        case 4:
            if (b->x == 8 && b->y == 14) b->masuk = 1;
            break;
    }
    if (b->masuk == 1 &&
        (((p->warna == 2 || p->warna == 4) && b->x == 7) ||
         ((p->warna == 1 || p->warna == 3) && b->y == 7))) {
        if (b->x == 7) {
            if (p->warna == 2 && b->y < 6) b->y++;
            else if (p->warna == 4 && b->y > 8) b->y--;
        } else {
            if (p->warna == 1 && b->x < 6) b->x++;
            else if (p->warna == 3 && b->x > 8) b->x--;
        }
    } else {
        if (b->x == 0) {
            if (b->y == 6) b->x++;
            else b->y--;
        } else if (b->x == 6) {
            if (b->y == 9) { b->x--; b->y--; }
            else if (b->y == 0) b->x++;
            else b->y--;
        } else if (b->x == 8) {
            if (b->y == 5) { b->x++; b->y++; }
            else if (b->y == 14) b->x--;
            else b->y++;
        } else if (b->x == 14) {
            if (b->y == 8) b->x--;
            else b->y++;
        } else if (b->y == 0) {
            if (b->x == 8) b->y++;
            else b->x++;
        } else if (b->y == 6) {
            if (b->x == 5) { b->y--; b->x++; }
            else if (b->x == 14) b->y++;
            else b->x++;
        } else if (b->y == 8) {
            if (b->x == 9) { b->y++; b->x--; }
            else if (b->x == 0) b->y--;
            else b->x--;
        } else if (b->y == 14) {
            if (b->x == 6) b->y--;
            else b->x--;
        }
    }
}

static void bidakMenang(Team *p, short pilihan) {
    Bidak *b = &bidak[p->warna - 1][pilihan - 1];
    switch (p->warna) {
        case 1:
            if (b->x == 6 && b->y == 7 && b->tamat == 0) {
                b->tamat = 1;
                if (green.outside[pilihan - 1] != 0) green.otsd = (green.otsd > 0) ? green.otsd - 1 : 0;
                green.outside[pilihan - 1] = 0;
                green.selesai++;
                msg(team_col(0), "GREEN PAWN %d FINISHED!", pilihan);
            }
            break;
        case 2:
            if (b->x == 7 && b->y == 6 && b->tamat == 0) {
                b->tamat = 1;
                if (yellow.outside[pilihan - 1] != 0) yellow.otsd = (yellow.otsd > 0) ? yellow.otsd - 1 : 0;
                yellow.outside[pilihan - 1] = 0;
                yellow.selesai++;
                msg(team_col(1), "YELLOW PAWN %d FINISHED!", pilihan);
            }
            break;
        case 3:
            if (b->x == 8 && b->y == 7 && b->tamat == 0) {
                b->tamat = 1;
                if (blue.outside[pilihan - 1] != 0) blue.otsd = (blue.otsd > 0) ? blue.otsd - 1 : 0;
                blue.outside[pilihan - 1] = 0;
                blue.selesai++;
                msg(team_col(2), "BLUE PAWN %d FINISHED!", pilihan);
            }
            break;
        case 4:
            if (b->x == 7 && b->y == 8 && b->tamat == 0) {
                b->tamat = 1;
                if (red.outside[pilihan - 1] != 0) red.otsd = (red.otsd > 0) ? red.otsd - 1 : 0;
                red.outside[pilihan - 1] = 0;
                red.selesai++;
                msg(team_col(3), "RED PAWN %d FINISHED!", pilihan);
            }
            break;
        default: break;
    }
}

static void pergerakan(Team *p, short kocokan, short pilihan) {
    Bidak *b = &bidak[p->warna - 1][pilihan - 1];
    if (b->tamat == 1) {
        msg(col(255, 190, 120, 255), "PAWN %d ALREADY FINISHED", pilihan);
        return;
    }
    if (g_pathRec) g_pathLen = 0;
    while (kocokan > 0) {
        langkahSatu(p, pilihan);
        if (g_pathRec && g_pathLen < PATH_MAX) {
            g_path[g_pathLen][0] = b->x;
            g_path[g_pathLen][1] = b->y;
            g_pathLen++;
        }
        kocokan--;
    }
    if ((b->x == 6 && b->y == 13) || (b->x == 8 && b->y == 12) ||
        (b->x == 13 && b->y == 8) || (b->x == 12 && b->y == 6) ||
        (b->x == 8 && b->y == 1) || (b->x == 6 && b->y == 2) ||
        (b->x == 1 && b->y == 6) || (b->x == 2 && b->y == 8)) {
        b->kebal = 1;
    } else {
        b->kebal = 0;
    }
    bidakMenang(p, pilihan);
}

typedef struct { int team, pawn; short fy, fx; } Victim;
static Victim g_victims[VICTIM_MAX];
static int g_victimCount = 0;

static int eliminasi(Team *p, short pilihan) {
    int elim = 0;
    Bidak *b = &bidak[p->warna - 1][pilihan - 1];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (bidak[i][j].x == b->x && bidak[i][j].y == b->y) {
                if (i == p->warna - 1) continue;
                if (bidak[i][j].kebal == 0) {
                    if (g_victimCount < VICTIM_MAX) {
                        g_victims[g_victimCount].team = i;
                        g_victims[g_victimCount].pawn = j;
                        g_victims[g_victimCount].fy = bidak[i][j].y;
                        g_victims[g_victimCount].fx = bidak[i][j].x;
                        g_victimCount++;
                    }
                    bidak[i][j].y = BASE_POS[i][j][0];
                    bidak[i][j].x = BASE_POS[i][j][1];
                    bidak[i][j].masuk = 0;
                    bidak[i][j].kebal = 0;
                    Team *target = teams[i];
                    target->outside[j] = 0;
                    target->inside[j] = (short)(j + 1);
                    target->insd++;
                    target->otsd--;
                    elim = 1;
                }
            }
        }
    }
    return elim;
}

static short cekKemenangan(short *pemenang) {
    if (green.selesai == 4) { *pemenang = 1; return 1; }
    if (yellow.selesai == 4) { *pemenang = 2; return 1; }
    if (blue.selesai == 4) { *pemenang = 3; return 1; }
    if (red.selesai == 4) { *pemenang = 4; return 1; }
    return 0;
}

static void pawnKeluar(short pilihan, Team *player) {
    Bidak *b = &bidak[player->warna - 1][pilihan - 1];
    switch (player->warna) {
        case 1: b->x = 1;  b->y = 6;  break;
        case 2: b->x = 8;  b->y = 1;  break;
        case 3: b->x = 13; b->y = 8;  break;
        case 4: b->x = 6;  b->y = 13; break;
    }
}

/* =========================================================
 *  Animations
 * ========================================================= */
enum { ANIM_NONE = 0, ANIM_MOVE, ANIM_RELEASE, ANIM_HOME };

typedef struct {
    int active;
    int kind;
    int team, pawn;
    double start, dur;
    int npts;
    Vec3 pts[ANIM_MAX];
    float hop;
} PawnAnim;

static PawnAnim g_anims[ANIM_MAX];

static void anims_clear(void) { memset(g_anims, 0, sizeof(g_anims)); }

static PawnAnim *anim_new(int kind, int team, int pawn) {
    for (int i = 0; i < ANIM_MAX; i++)
        if (!g_anims[i].active) {
            PawnAnim *a = &g_anims[i];
            memset(a, 0, sizeof(*a));
            a->active = 1;
            a->kind = kind;
            a->team = team;
            a->pawn = pawn;
            a->start = g_now;
            a->dur = 0.3;
            a->hop = 0.25f;
            return a;
        }
    return NULL;
}

static PawnAnim *anim_find(int team, int pawn) {
    for (int i = 0; i < ANIM_MAX; i++)
        if (g_anims[i].active && g_anims[i].team == team && g_anims[i].pawn == pawn)
            return &g_anims[i];
    return NULL;
}

static void anim_add_cell(PawnAnim *a, short gx, short gy) {
    if (a->npts >= ANIM_MAX) return;
    a->pts[a->npts++] = cell_world(gx, gy);
}

static int anim_ground_pos(const PawnAnim *a, Vec3 *out, float *hopFrac) {
    if (!a->active) return 0;
    double f = (g_now - a->start) / a->dur;
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    int segs = a->npts - 1;
    if (segs < 1) { *out = a->pts[0]; *hopFrac = 0; return 1; }
    float sf = (float)f * segs;
    int seg = (int)sf;
    if (seg > segs - 1) seg = segs - 1;
    float u = sf - seg;
    *out = vlerp(a->pts[seg], a->pts[seg + 1], u);
    *hopFrac = sinf(u * PI);
    return 1;
}

static int anim_pos(const PawnAnim *a, Vec3 *out) {
    float hf;
    if (!anim_ground_pos(a, out, &hf)) return 0;
    out->y += hf * a->hop;
    return 1;
}

static int anims_running(void) {
    for (int i = 0; i < ANIM_MAX; i++)
        if (g_anims[i].active) return 1;
    return 0;
}

/* =========================================================
 *  Game state machine
 * ========================================================= */
enum {
    PH_MENU = 0,
    PH_CONFIRM_EXIT,
    PH_INTRO_WHO,
    PH_INTRO_FIRST,
    PH_TURN_BEGIN,
    PH_HUMAN_WAIT_ROLL,
    PH_DICE,
    PH_HUMAN_CHOICE,
    PH_HUMAN_PICK_FREE,
    PH_HUMAN_PICK_MOVE,
    PH_ANIM_MOVE,
    PH_ANIM_VICTIMS,
    PH_ANIM_RELEASE,
    PH_BOT_THINK,
    PH_WAIT,
    PH_TURN_END,
    PH_GAME_OVER
};

enum { AM_END = 0, AM_REROLL = 1 };
enum { BOTACT_NONE = 0, BOTACT_ROLL, BOTACT_RELEASE, BOTACT_CHOICE, BOTACT_MOVE };

static int g_phase = PH_MENU;
static double g_phaseT = 0.0;
static double g_deadline = 0.0;
static int g_waitNext = PH_MENU;
static int g_running = 1;

static int g_turn = 0;
static int g_player = 0;
static int g_roll = 0;
static int g_winner = -1;
static int g_isBotRoll = 0;
static int g_afterMove = AM_END;
static int g_inExtra = 0;
static int g_savedAfter = AM_END;
static int g_elimThisMove = 0;
static int g_lastPawn = -1;
static int g_botAct = BOTACT_NONE;
static int g_confirmFrom = PH_MENU;
static int g_autoHuman = 0;

static int g_click = 0, g_clickX = 0, g_clickY = 0;
static int g_mouseX = 0, g_mouseY = 0;
static int g_drag = 0, g_lastMX = 0, g_lastMY = 0;
static int g_hoverTeam = -1, g_hoverPawn = -1;

static float g_dieCur[3] = {0, 0, 0};

static void confetti_init(void);
static void confetti_update(void);

typedef struct { double start, dur; float from[3], to[3]; } DiceAnim;
static DiceAnim g_dice;

static float ang_delta(float from, float to) {
    float d = fmodf(to - from, TAU);
    if (d < 0) d += TAU;
    return d;
}

static void enter_phase(int ph) {
    g_phase = ph;
    g_phaseT = g_now;
    switch (ph) {
        case PH_INTRO_WHO:
        case PH_INTRO_FIRST:
            g_deadline = g_now + 1.7;
            break;
        case PH_TURN_BEGIN:
            g_deadline = g_now + 0.55;
            if (g_turn == g_player) msg(team_col(g_turn), "YOUR TURN");
            else msg(team_col(g_turn), "BOT (%s) TURN", TEAM_NAMES[g_turn]);
            break;
        case PH_TURN_END:
            g_deadline = g_now + 0.7;
            break;
        case PH_GAME_OVER:
            g_deadline = g_now + 1.0;
            confetti_init();
            break;
        default:
            break;
    }
}

static void wait_then(double delay, int next) {
    g_waitNext = next;
    g_deadline = g_now + delay;
    enter_phase(PH_WAIT);
}

static void enter_bot_act(int act, double delay) {
    g_botAct = act;
    g_deadline = g_now + delay;
    enter_phase(PH_BOT_THINK);
}

static int is_selectable(int t, int i) {
    if (t != g_turn) return 0;
    if (g_phase == PH_HUMAN_PICK_FREE) return teams[t]->inside[i] != 0;
    if (g_phase == PH_HUMAN_PICK_MOVE) return teams[t]->outside[i] != 0 && bidak[t][i].tamat == 0;
    return 0;
}

static void go_turn_end(void) { enter_phase(PH_TURN_END); }

static void go_roll_phase(double delay) {
    if (g_turn == g_player) wait_then(delay, PH_HUMAN_WAIT_ROLL);
    else enter_bot_act(BOTACT_ROLL, delay);
}

static void die_rest_euler(int v, float out[3]) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    switch (v) {
        case 1: break;
        case 2: out[0] = -PI / 2.0f; break;
        case 3: out[2] = PI / 2.0f; break;
        case 4: out[2] = -PI / 2.0f; break;
        case 5: out[0] = PI / 2.0f; break;
        default: out[0] = PI; break;
    }
}

static void start_dice(int bot) {
    g_roll = dadu();
    g_isBotRoll = bot;
    g_dice.start = g_now;
    g_dice.dur = bot ? 0.75 : 1.05;
    for (int k = 0; k < 3; k++) g_dice.from[k] = g_dieCur[k];

    float rest[3];
    die_rest_euler(g_roll, rest);
    float ryTarget = (float)(rand() % 360) * PI / 180.0f;
    g_dice.to[0] = g_dice.from[0] + (2 + rand() % 2) * TAU + ang_delta(g_dice.from[0], rest[0]);
    g_dice.to[1] = g_dice.from[1] + (4 + rand() % 3) * TAU + ang_delta(g_dice.from[1], ryTarget);
    g_dice.to[2] = g_dice.from[2] + (2 + rand() % 2) * TAU + ang_delta(g_dice.from[2], rest[2]);
    enter_phase(PH_DICE);
}

static void do_release(int team, int pa) {
    Team *p = teams[team];
    short oy = bidak[team][pa].y, ox = bidak[team][pa].x;
    p->inside[pa] = 0;
    p->outside[pa] = (short)(pa + 1);
    p->insd--;
    p->otsd++;
    pawnKeluar((short)(pa + 1), p);
    PawnAnim *a = anim_new(ANIM_RELEASE, team, pa);
    if (a) {
        a->dur = 0.5;
        a->hop = 0.45f;
        anim_add_cell(a, ox, oy);
        anim_add_cell(a, bidak[team][pa].x, bidak[team][pa].y);
    }
    enter_phase(PH_ANIM_RELEASE);
}

static void do_move(int team, int pa, int steps) {
    short sy = bidak[team][pa].y, sx = bidak[team][pa].x;
    g_lastPawn = pa;
    g_pathRec = 1;
    g_pathLen = 0;
    pergerakan(teams[team], (short)steps, (short)(pa + 1));
    g_pathRec = 0;
    PawnAnim *a = anim_new(ANIM_MOVE, team, pa);
    if (a) {
        a->hop = 0.28f;
        anim_add_cell(a, sx, sy);
        for (int k = 0; k < g_pathLen; k++) anim_add_cell(a, g_path[k][0], g_path[k][1]);
        int segs = a->npts - 1;
        if (segs < 1) segs = 1;
        a->dur = 0.13 * segs + 0.08;
        if (a->dur < 0.25) a->dur = 0.25;
    }
    enter_phase(PH_ANIM_MOVE);
}

static void after_move_continuation(void) {
    if (g_elimThisMove) {
        if (!g_inExtra) { g_inExtra = 1; g_savedAfter = g_afterMove; }
        if (g_turn == g_player) msg(col(255, 200, 90, 255), "YOU ELIMINATED AN OPPONENT! EXTRA ROLL");
        else msg(team_col(g_turn), "BOT ELIMINATED AN OPPONENT! EXTRA ROLL");
        go_roll_phase(0.8);
    } else {
        if (g_inExtra) { g_inExtra = 0; g_afterMove = g_savedAfter; }
        if (g_afterMove == AM_REROLL) go_roll_phase(0.45);
        else go_turn_end();
    }
}

static void move_finished(void) {
    g_victimCount = 0;
    int elim = eliminasi(teams[g_turn], (short)(g_lastPawn + 1));
    if (elim) {
        for (int v = 0; v < g_victimCount; v++) {
            PawnAnim *a = anim_new(ANIM_HOME, g_victims[v].team, g_victims[v].pawn);
            if (a) {
                a->dur = 0.6;
                a->hop = 1.4f;
                anim_add_cell(a, g_victims[v].fx, g_victims[v].fy);
                anim_add_cell(a, bidak[g_victims[v].team][g_victims[v].pawn].x,
                                 bidak[g_victims[v].team][g_victims[v].pawn].y);
            }
        }
        msg(col(255, 140, 120, 255), "OPPONENT PAWN SENT HOME!");
        g_elimThisMove = 1;
        enter_phase(PH_ANIM_VICTIMS);
    } else {
        g_elimThisMove = 0;
        after_move_continuation();
    }
}

static void human_resolve_roll(void) {
    Team *p = teams[g_turn];
    if (g_inExtra) {
        if (p->otsd > 0) {
            enter_phase(PH_HUMAN_PICK_MOVE);
        } else {
            g_elimThisMove = 0;
            after_move_continuation();
        }
        return;
    }
    if (p->otsd == 0) {
        if (g_roll != 6) {
            msg(col(255, 170, 120, 255), "YOU NEED A 6 TO LEAVE BASE - TURN SKIPPED");
            go_turn_end();
        } else if (p->insd > 0) {
            g_afterMove = AM_REROLL;
            msg(col(120, 230, 255, 255), "ROLLED 6! PICK A PAWN TO SET FREE");
            enter_phase(PH_HUMAN_PICK_FREE);
        } else {
            msg(col(255, 170, 120, 255), "NO PAWN LEFT TO RELEASE - TURN SKIPPED");
            go_turn_end();
        }
    } else if (g_roll == 6) {
        if (p->otsd < 4 && p->insd > 0) {
            enter_phase(PH_HUMAN_CHOICE);
        } else {
            g_afterMove = AM_REROLL;
            msg(col(120, 230, 255, 255), "ROLLED 6! PICK A PAWN TO MOVE");
            enter_phase(PH_HUMAN_PICK_MOVE);
        }
    } else {
        g_afterMove = AM_END;
        msg(col(220, 225, 240, 255), "PICK A PAWN TO MOVE");
        enter_phase(PH_HUMAN_PICK_MOVE);
    }
}

static void bot_pick_and_move(void) {
    Team *p = teams[g_turn];
    int pa = -1;
    for (int tries = 0; tries < 8 && pa < 0; tries++) {
        int c = rand() % 4;
        if (p->outside[c] != 0 && bidak[g_turn][c].tamat == 0) pa = c;
    }
    if (pa < 0)
        for (int i = 0; i < 4; i++)
            if (p->outside[i] != 0 && bidak[g_turn][i].tamat == 0) { pa = i; break; }
    if (pa < 0) { go_turn_end(); return; }
    msg(team_col(g_turn), "BOT MOVES PAWN %d BY %d", pa + 1, g_roll);
    do_move(g_turn, pa, g_roll);
}

static void bot_resolve_roll(void) {
    Team *p = teams[g_turn];
    if (g_inExtra) { enter_bot_act(BOTACT_MOVE, 0.45); return; }
    if (p->otsd == 0) {
        if (g_roll != 6) {
            msg(team_col(g_turn), "BOT (%s) NEEDS A 6 - TURN SKIPPED", TEAM_NAMES[g_turn]);
            go_turn_end();
        } else {
            g_afterMove = AM_REROLL;
            enter_bot_act(BOTACT_RELEASE, 0.6);
        }
    } else {
        g_afterMove = (g_roll == 6) ? AM_REROLL : AM_END;
        if (g_roll == 6 && p->otsd < 4 && p->insd > 0) enter_bot_act(BOTACT_CHOICE, 0.6);
        else enter_bot_act(BOTACT_MOVE, 0.6);
    }
}

static void bot_execute_act(void) {
    Team *p = teams[g_turn];
    if (g_botAct == BOTACT_ROLL) { start_dice(1); return; }
    if (g_botAct == BOTACT_RELEASE) {
        int cand[4], n = 0;
        for (int i = 0; i < 4; i++) if (p->inside[i]) cand[n++] = i;
        if (n == 0) { g_afterMove = AM_END; go_turn_end(); return; }
        int pa = cand[rand() % n];
        msg(team_col(g_turn), "BOT SETS PAWN %d FREE", pa + 1);
        do_release(g_turn, pa);
        return;
    }
    if (g_botAct == BOTACT_CHOICE) {
        if (p->insd > 0 && rand() % 2 == 0) {
            int cand[4], n = 0;
            for (int i = 0; i < 4; i++) if (p->inside[i]) cand[n++] = i;
            int pa = cand[rand() % n];
            msg(team_col(g_turn), "BOT SETS PAWN %d FREE", pa + 1);
            do_release(g_turn, pa);
        } else {
            bot_pick_and_move();
        }
        return;
    }
    if (g_botAct == BOTACT_MOVE) bot_pick_and_move();
}

static void game_reset(void) {
    green = set(1);
    yellow = set(2);
    blue = set(3);
    red = set(4);
    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 4; i++) {
            bidak[t][i].masuk = 0;
            bidak[t][i].kebal = 0;
            bidak[t][i].tamat = 0;
            bidak[t][i].y = BASE_POS[t][i][0];
            bidak[t][i].x = BASE_POS[t][i][1];
            float wx = (float)bidak[t][i].x - 7.0f;
            float wz = (float)bidak[t][i].y - 7.0f;
            g_pawnYaw[t][i] = (wx == 0 && wz == 0) ? 0.0f : atan2f(-wx, -wz);
        }
}

static void start_new_game(void) {
    game_reset();
    g_player = rand() % 4;
    g_turn = rand() % 4;
    g_winner = -1;
    g_roll = 0;
    g_inExtra = 0;
    g_savedAfter = AM_END;
    g_afterMove = AM_END;
    g_elimThisMove = 0;
    g_lastPawn = -1;
    g_isBotRoll = 0;
    g_botAct = BOTACT_NONE;
    anims_clear();
    g_logCount = 0;
    g_banner[0] = '\0';
    g_bannerT = -10.0;
    g_dieCur[0] = g_dieCur[1] = g_dieCur[2] = 0;
    camYaw = -0.65f;
    camPitch = 0.82f;
    camDist = 24.5f;
    enter_phase(PH_INTRO_WHO);
    msg(team_col(g_player), "YOU PLAY AS %s", TEAM_NAMES[g_player]);
}

static void auto_human_decide(void) {
    if (!g_autoHuman || g_turn != g_player) return;
    if (g_now < g_phaseT + 0.05) return;
    if (g_phase == PH_HUMAN_WAIT_ROLL) { start_dice(0); return; }
    if (g_phase == PH_HUMAN_CHOICE) {
        Team *p = teams[g_turn];
        if (p->insd > 0 && rand() % 2 == 0) {
            g_afterMove = AM_REROLL;
            enter_phase(PH_HUMAN_PICK_FREE);
        } else {
            g_afterMove = AM_REROLL;
            enter_phase(PH_HUMAN_PICK_MOVE);
        }
        return;
    }
    if (g_phase == PH_HUMAN_PICK_FREE) {
        Team *p = teams[g_turn];
        int cand[4], n = 0;
        for (int i = 0; i < 4; i++) if (p->inside[i]) cand[n++] = i;
        if (n) do_release(g_turn, cand[rand() % n]);
        else if (p->otsd > 0) bot_pick_and_move();
        else go_turn_end();
        return;
    }
    if (g_phase == PH_HUMAN_PICK_MOVE) bot_pick_and_move();
}

static void update_logic(void) {
    if (g_phase == PH_MENU) camYaw += g_dt * 0.10f;
    if (g_phase == PH_GAME_OVER) {
        camYaw += g_dt * 0.05f;
        confetti_update();
    }
    auto_human_decide();

    for (int i = 0; i < ANIM_MAX; i++)
        if (g_anims[i].active && g_now >= g_anims[i].start + g_anims[i].dur)
            g_anims[i].active = 0;

    switch (g_phase) {
        case PH_INTRO_WHO:
            if (g_now >= g_deadline) {
                msg(team_col(g_turn), "%s GOES FIRST", TEAM_NAMES[g_turn]);
                enter_phase(PH_INTRO_FIRST);
            }
            break;

        case PH_INTRO_FIRST:
            if (g_now >= g_deadline) enter_phase(PH_TURN_BEGIN);
            break;

        case PH_TURN_BEGIN:
            if (g_now >= g_deadline) {
                if (g_turn == g_player) enter_phase(PH_HUMAN_WAIT_ROLL);
                else enter_bot_act(BOTACT_ROLL, 0.7);
            }
            break;

        case PH_DICE: {
            if (g_now >= g_dice.start + g_dice.dur) {
                for (int k = 0; k < 3; k++) g_dieCur[k] = g_dice.to[k];
                if (g_isBotRoll) msg(team_col(g_turn), "BOT (%s) ROLLED %d", TEAM_NAMES[g_turn], g_roll);
                else msg(col(255, 235, 140, 255), "YOU ROLLED %d", g_roll);
                if (g_isBotRoll) bot_resolve_roll();
                else human_resolve_roll();
            } else {
                float f = (float)((g_now - g_dice.start) / g_dice.dur);
                float e = 1.0f - powf(1.0f - f, 3.0f);
                for (int k = 0; k < 3; k++)
                    g_dieCur[k] = g_dice.from[k] + (g_dice.to[k] - g_dice.from[k]) * e;
            }
            break;
        }

        case PH_ANIM_MOVE:
            if (!anims_running()) move_finished();
            break;

        case PH_ANIM_VICTIMS:
            if (!anims_running()) after_move_continuation();
            break;

        case PH_ANIM_RELEASE:
            if (!anims_running()) {
                if (g_afterMove == AM_REROLL) go_roll_phase(0.35);
                else go_turn_end();
            }
            break;

        case PH_BOT_THINK:
            if (g_now >= g_deadline) bot_execute_act();
            break;

        case PH_WAIT:
            if (g_now >= g_deadline) enter_phase(g_waitNext);
            break;

        case PH_TURN_END:
            if (g_now >= g_deadline) {
                short w = 0;
                if (cekKemenangan(&w)) {
                    g_winner = w - 1;
                    enter_phase(PH_GAME_OVER);
                } else {
                    g_turn = (g_turn + 1) % 4;
                    g_roll = 0;
                    enter_phase(PH_TURN_BEGIN);
                }
            }
            break;

        default:
            break;
    }
}

/* =========================================================
 *  UI helpers
 * ========================================================= */
enum {
    BTN_NONE = 0, BTN_PLAY, BTN_EXIT, BTN_YES, BTN_NO,
    BTN_ROLL, BTN_FREE, BTN_MOVE, BTN_MENU
};

typedef struct {
    SDL_Rect r;
    const char *label;
    int id;
    int enabled;
    int primary;
} Btn;

static Btn g_btns[6];
static int g_btnCount = 0;
static int g_hoverBtn = -1;

static void fill_rect(SDL_Renderer *r, SDL_Rect rc, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rc);
}

static void border_rect(SDL_Renderer *r, SDL_Rect rc, SDL_Color c) {
    fill_rect(r, (SDL_Rect){rc.x, rc.y, rc.w, 1}, c);
    fill_rect(r, (SDL_Rect){rc.x, rc.y + rc.h - 1, rc.w, 1}, c);
    fill_rect(r, (SDL_Rect){rc.x, rc.y, 1, rc.h}, c);
    fill_rect(r, (SDL_Rect){rc.x + rc.w - 1, rc.y, 1, rc.h}, c);
}

static void draw_panel(SDL_Renderer *r, SDL_Rect rc, Uint8 a) {
    fill_rect(r, rc, col(12, 14, 22, a));
    border_rect(r, rc, col(96, 108, 140, 200));
}

static int point_in_rect(int x, int y, SDL_Rect rc) {
    return x >= rc.x && x < rc.x + rc.w && y >= rc.y && y < rc.y + rc.h;
}

static void add_btn(SDL_Rect r, const char *label, int id, int enabled, int primary) {
    if (g_btnCount >= 6) return;
    g_btns[g_btnCount].r = r;
    g_btns[g_btnCount].label = label;
    g_btns[g_btnCount].id = id;
    g_btns[g_btnCount].enabled = enabled;
    g_btns[g_btnCount].primary = primary;
    g_btnCount++;
}

static void layout_buttons(void) {
    g_btnCount = 0;
    switch (g_phase) {
        case PH_MENU:
            add_btn((SDL_Rect){WIN_W / 2 - 150, 400, 300, 64}, "PLAY", BTN_PLAY, 1, 1);
            add_btn((SDL_Rect){WIN_W / 2 - 150, 484, 300, 64}, "EXIT", BTN_EXIT, 1, 0);
            break;
        case PH_CONFIRM_EXIT:
            add_btn((SDL_Rect){WIN_W / 2 - 220, 450, 200, 60}, "YES", BTN_YES, 1, 1);
            add_btn((SDL_Rect){WIN_W / 2 + 20, 450, 200, 60}, "NO", BTN_NO, 1, 0);
            break;
        case PH_HUMAN_WAIT_ROLL:
            add_btn((SDL_Rect){WIN_W / 2 - 170, WIN_H - 100, 340, 66}, "ROLL DICE  [R]", BTN_ROLL, 1, 1);
            break;
        case PH_HUMAN_CHOICE:
            add_btn((SDL_Rect){WIN_W / 2 - 260, WIN_H - 100, 250, 66}, "SET PAWN FREE", BTN_FREE,
                    teams[g_turn]->insd > 0, 0);
            add_btn((SDL_Rect){WIN_W / 2 + 10, WIN_H - 100, 250, 66}, "MOVE PAWN", BTN_MOVE, 1, 0);
            break;
        case PH_GAME_OVER:
            add_btn((SDL_Rect){WIN_W / 2 - 160, 560, 320, 64}, "BACK TO MENU", BTN_MENU, 1, 1);
            break;
        default:
            break;
    }
}

static void draw_buttons(SDL_Renderer *r) {
    for (int i = 0; i < g_btnCount; i++) {
        Btn *b = &g_btns[i];
        SDL_Color base = b->primary ? team_col(g_turn) : col(70, 78, 102, 255);
        if (!b->enabled) base = col(45, 48, 58, 255);
        else if (i == g_hoverBtn) base = shade_color(base, 1.3f);
        fill_rect(r, b->r, base);
        border_rect(r, b->r, shade_color(base, 1.45f));
        SDL_Color tc = b->enabled ? col(255, 255, 255, 255) : col(120, 120, 130, 255);
        draw_text_center(r, (float)b->r.x + b->r.w * 0.5f, (float)b->r.y + b->r.h * 0.5f - 7.0f,
                         2.0f, tc, b->label);
    }
}

/* =========================================================
 *  Selection rings (screen-space overlay)
 * ========================================================= */
static void draw_selection_overlays(SDL_Renderer *r) {
    SDL_Vertex ov[4 * 24 * 6];
    int n = 0;
    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 4; i++) {
            if (!is_selectable(t, i)) continue;
            Vec3 c = cell_world(bidak[t][i].x, bidak[t][i].y);
            c.y += 0.03f;
            float pulse = 0.5f + 0.5f * sinf((float)g_now * 6.0f);
            int hovered = (t == g_hoverTeam && i == g_hoverPawn);
            SDL_Color rc = hovered ? col(255, 255, 255, 235)
                                   : col(255, 230, 90, (int)(150 + 80 * pulse));
            const int SEG = 24;
            float rin = hovered ? 0.30f : 0.33f;
            float rout = hovered ? 0.46f : 0.42f;
            Vec3 prev_o, prev_i;
            int prev_ok = 0;
            for (int s = 0; s <= SEG; s++) {
                float a = TAU * s / SEG;
                Vec3 po = v3(c.x + cosf(a) * rout, c.y, c.z + sinf(a) * rout);
                Vec3 pi = v3(c.x + cosf(a) * rin, c.y, c.z + sinf(a) * rin);
                float o0x = 0, o0y = 0, i0x = 0, i0y = 0;
                int ok = project_point(po, &o0x, &o0y) && project_point(pi, &i0x, &i0y);
                if (prev_ok && ok) {
                    ov[n].position.x = prev_o.x; ov[n].position.y = prev_o.y; ov[n].color = rc; ov[n].tex_coord.x = 0; ov[n].tex_coord.y = 0; n++;
                    ov[n].position.x = o0x;      ov[n].position.y = o0y;      ov[n].color = rc; ov[n].tex_coord.x = 0; ov[n].tex_coord.y = 0; n++;
                    ov[n].position.x = i0x;      ov[n].position.y = i0y;      ov[n].color = rc; ov[n].tex_coord.x = 0; ov[n].tex_coord.y = 0; n++;
                    ov[n].position.x = prev_o.x; ov[n].position.y = prev_o.y; ov[n].color = rc; ov[n].tex_coord.x = 0; ov[n].tex_coord.y = 0; n++;
                    ov[n].position.x = i0x;      ov[n].position.y = i0y;      ov[n].color = rc; ov[n].tex_coord.x = 0; ov[n].tex_coord.y = 0; n++;
                    ov[n].position.x = prev_i.x; ov[n].position.y = prev_i.y; ov[n].color = rc; ov[n].tex_coord.x = 0; ov[n].tex_coord.y = 0; n++;
                }
                prev_o.x = o0x; prev_o.y = o0y;
                prev_i.x = i0x; prev_i.y = i0y;
                prev_ok = ok;
            }
        }
    if (n > 0) SDL_RenderGeometry(r, NULL, ov, n, NULL, 0);
}

/* =========================================================
 *  3D scene
 * ========================================================= */
static Vec3 die_pos_for_team(int t) {
    switch (t) {
        case 0: return v3(-5.4f, 1.15f, -5.4f);
        case 1: return v3(5.4f, 1.15f, -5.4f);
        case 2: return v3(5.4f, 1.15f, 5.4f);
        default: return v3(-5.4f, 1.15f, 5.4f);
    }
}

static void render_scene_3d(void) {
    g_stn = 0;
    SDL_Color white = col(255, 255, 255, 255);
    SDL_Color shc = col(22, 22, 30, 255);
    render_mesh(&g_board, mat_identity(), white);

    /* soft blob shadows under pawns */
    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 4; i++) {
            Vec3 gp;
            float rs = 0.33f;
            PawnAnim *a = anim_find(t, i);
            if (a) {
                float hf;
                if (!anim_ground_pos(a, &gp, &hf)) continue;
                rs *= 1.0f - 0.35f * hf;
            } else {
                gp = cell_world(bidak[t][i].x, bidak[t][i].y);
            }
            Mat4 m = mat_mul(mat_translate(gp.x, gp.y + 0.012f, gp.z), mat_scale(rs, 1, rs));
            render_mesh(&g_shadow, m, shc);
        }

    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 4; i++) {
            Vec3 pos;
            PawnAnim *a = anim_find(t, i);
            if (a) {
                if (!anim_pos(a, &pos)) continue;
            } else {
                pos = cell_world(bidak[t][i].x, bidak[t][i].y);
            }
            float s = 1.0f;
            if (a == NULL && is_selectable(t, i)) {
                pos.y += 0.05f * sinf((float)g_now * 5.0f + i * 1.7f);
                s += 0.04f * sinf((float)g_now * 5.0f + i);
            }
            if (t == g_hoverTeam && i == g_hoverPawn) s = 1.18f;
            Mat4 m = mat_mul(mat_translate(pos.x, pos.y, pos.z),
                             mat_mul(mat_scale(s, s, s), mat_rot_y(g_pawnYaw[t][i])));
            render_mesh(&g_pawn, m, team_col(t));
        }

    Vec3 dp = die_pos_for_team(g_turn);
    if (g_phase == PH_DICE) {
        float f = (float)((g_now - g_dice.start) / g_dice.dur);
        if (f > 1) f = 1;
        dp.y += fabsf(sinf(f * PI * 2.0f)) * (1.0f - f) * 0.6f;
    }

    /* die shadow */
    {
        int dgx = (int)lroundf(dp.x + 7.0f), dgy = (int)lroundf(dp.z + 7.0f);
        if (dgx < 0) dgx = 0;
        if (dgx > 14) dgx = 14;
        if (dgy < 0) dgy = 0;
        if (dgy > 14) dgy = 14;
        float bounce = dp.y - 1.15f;
        float drs = 0.55f * (1.0f - 0.4f * (bounce / 0.6f));
        if (drs < 0.2f) drs = 0.2f;
        Mat4 sm = mat_mul(mat_translate(dp.x, cell_top(dgy, dgx) + 0.012f, dp.z),
                          mat_scale(drs, 1, drs));
        render_mesh(&g_shadow, sm, shc);
    }

    Mat4 dm = mat_mul(mat_translate(dp.x, dp.y, dp.z),
                      mat_mul(mat_rot_y(g_dieCur[1]),
                              mat_mul(mat_rot_x(g_dieCur[0]), mat_rot_z(g_dieCur[2]))));
    render_mesh(&g_die, dm, white);
}

/* =========================================================
 *  HUD / screens
 * ========================================================= */
static void draw_hud(SDL_Renderer *r) {
    SDL_Rect tl = {16, 14, 330, 58};
    draw_panel(r, tl, 210);
    fill_rect(r, (SDL_Rect){30, 26, 34, 34}, team_col(g_turn));
    border_rect(r, (SDL_Rect){30, 26, 34, 34}, col(255, 255, 255, 180));
    if (g_turn == g_player) draw_text(r, 76, 22, 3, col(255, 255, 255, 255), "YOUR TURN");
    else draw_text(r, 76, 22, 3, col(255, 255, 255, 255), "BOT TURN");
    draw_text(r, 78, 48, 2, team_col(g_turn), TEAM_NAMES[g_turn]);

    SDL_Rect tr = {WIN_W - 360, 14, 344, 58};
    draw_panel(r, tr, 210);
    draw_text(r, WIN_W - 346, 22, 2, col(180, 190, 215, 255), "FINISH");
    for (int t = 0; t < 4; t++) {
        int x = WIN_W - 346 + t * 84;
        fill_rect(r, (SDL_Rect){x, 44, 18, 18}, team_col(t));
        char buf[16];
        snprintf(buf, sizeof(buf), "%d/4", teams[t]->selesai);
        draw_text(r, (float)(x + 24), 44, 2, col(230, 230, 240, 255), buf);
    }

    if (g_roll > 0) {
        char dbuf[24];
        snprintf(dbuf, sizeof(dbuf), "DICE %d", g_roll);
        draw_text_center(r, WIN_W * 0.5f, 24, 3, col(255, 235, 140, 255), dbuf);
    }

    SDL_Rect lp = {16, 84, 520, 168};
    draw_panel(r, lp, 200);
    draw_text(r, (float)(lp.x + 12), (float)(lp.y + 8), 2, col(160, 170, 200, 255), "MATCH LOG");
    int show = g_logCount < MSG_MAX ? g_logCount : MSG_MAX;
    for (int s = 0; s < show; s++) {
        int idx = (g_logCount - show + s) % MSG_MAX;
        SDL_Color c = g_logCol[idx];
        if (s < show - 1) c = shade_color(c, 0.72f);
        draw_text(r, (float)(lp.x + 12), (float)(lp.y + 36 + s * 21), 2, c, g_log[idx]);
    }

    draw_text(r, WIN_W - 452, WIN_H - 38, 2, col(140, 150, 180, 255),
              "DRAG ROTATE   WHEEL ZOOM   ESC MENU");

    if (g_phase == PH_HUMAN_PICK_FREE || g_phase == PH_HUMAN_PICK_MOVE) {
        draw_text_center(r, WIN_W * 0.5f, WIN_H - 130, 2, col(255, 230, 120, 255),
                         "CLICK A GLOWING PAWN (OR PRESS 1-4)");
    }
}

static void draw_banner(SDL_Renderer *r) {
    double age = g_now - g_bannerT;
    if (age > 2.4 || g_banner[0] == '\0') return;
    Uint8 a = 255;
    if (age > 1.8) a = (Uint8)(255.0 * (2.4 - age) / 0.6);
    int w = (int)text_w(g_banner, 3.0f) + 80;
    SDL_Rect rc = {WIN_W / 2 - w / 2, 96, w, 52};
    fill_rect(r, rc, col(10, 12, 20, (int)(a * 0.75)));
    border_rect(r, rc, col(g_bannerCol.r, g_bannerCol.g, g_bannerCol.b, a));
    SDL_Color c = g_bannerCol;
    c.a = a;
    draw_text_center(r, WIN_W * 0.5f, 110, 3, c, g_banner);
}

static void draw_menu(SDL_Renderer *r) {
    fill_rect(r, (SDL_Rect){0, 0, WIN_W, WIN_H}, col(6, 8, 14, 105));
    draw_text_center(r, WIN_W * 0.5f, 160, 10, col(255, 214, 80, 255), "LUDO 3D");
    draw_text_center(r, WIN_W * 0.5f, 280, 2, col(210, 216, 232, 255), "CLASSIC LUDO - 3D SDL REMAKE");
    draw_text_center(r, WIN_W * 0.5f, 316, 2, col(160, 170, 200, 255), "4 PAWNS - 4 TEAMS - 1 WINNER");
    draw_text_center(r, WIN_W * 0.5f, 620, 2, col(140, 150, 180, 255),
                     "MOUSE DRAG ROTATE - WHEEL ZOOM - R ROLL - ESC MENU");
}

static void draw_confirm(SDL_Renderer *r) {
    fill_rect(r, (SDL_Rect){0, 0, WIN_W, WIN_H}, col(6, 8, 14, 175));
    SDL_Rect rc = {WIN_W / 2 - 330, 330, 660, 200};
    draw_panel(r, rc, 245);
    draw_text_center(r, WIN_W * 0.5f, 366, 3, col(255, 255, 255, 255), "EXIT THE GAME?");
    draw_text_center(r, WIN_W * 0.5f, 412, 2, col(200, 205, 220, 255),
                     "ANY PROGRESS WON'T BE SAVED!");
}

/* confetti */
#define CONF_N 130
typedef struct { float x, y, vx, vy, size; SDL_Color c; } Conf;
static Conf g_conf[CONF_N];

static void confetti_init(void) {
    for (int i = 0; i < CONF_N; i++) {
        g_conf[i].x = (float)(rand() % WIN_W);
        g_conf[i].y = (float)(-rand() % WIN_H);
        g_conf[i].vx = (float)(rand() % 200 - 100) * 0.5f;
        g_conf[i].vy = (float)(90 + rand() % 180);
        g_conf[i].size = (float)(5 + rand() % 7);
        g_conf[i].c = (i % 5 == 0) ? col(255, 255, 255, 255) : team_col(rand() % 4);
    }
}

static void confetti_update(void) {
    for (int i = 0; i < CONF_N; i++) {
        g_conf[i].y += g_conf[i].vy * g_dt;
        g_conf[i].x += g_conf[i].vx * g_dt;
        if (g_conf[i].y > WIN_H + 20) {
            g_conf[i].y = -20;
            g_conf[i].x = (float)(rand() % WIN_W);
        }
    }
}

static void draw_confetti(SDL_Renderer *r) {
    for (int i = 0; i < CONF_N; i++) {
        SDL_Rect rc;
        rc.x = (int)g_conf[i].x;
        rc.y = (int)g_conf[i].y;
        rc.w = (int)g_conf[i].size;
        rc.h = (int)(g_conf[i].size * 0.5f) + 1;
        fill_rect(r, rc, g_conf[i].c);
    }
}

static void draw_gameover(SDL_Renderer *r) {
    draw_confetti(r);
    fill_rect(r, (SDL_Rect){0, 0, WIN_W, WIN_H}, col(6, 8, 14, 120));
    char buf[64];
    snprintf(buf, sizeof(buf), "%s WINS THE ROUND!", TEAM_NAMES[g_winner]);
    draw_text_center(r, WIN_W * 0.5f, 300, 6, team_col(g_winner), buf);
    draw_text_center(r, WIN_W * 0.5f, 400, 3, col(230, 230, 240, 255), "GREAT GAME!");
}

/* =========================================================
 *  Input
 * ========================================================= */
static void button_action(int id) {
    switch (id) {
        case BTN_PLAY:
            start_new_game();
            break;
        case BTN_EXIT:
            g_confirmFrom = g_phase;
            enter_phase(PH_CONFIRM_EXIT);
            break;
        case BTN_YES:
            if (g_confirmFrom == PH_MENU) g_running = 0;
            else enter_phase(PH_MENU);
            break;
        case BTN_NO:
            enter_phase(g_confirmFrom);
            break;
        case BTN_ROLL:
            start_dice(0);
            break;
        case BTN_FREE:
            g_afterMove = AM_REROLL;
            enter_phase(PH_HUMAN_PICK_FREE);
            break;
        case BTN_MOVE:
            g_afterMove = AM_REROLL;
            enter_phase(PH_HUMAN_PICK_MOVE);
            break;
        case BTN_MENU:
            enter_phase(PH_MENU);
            break;
        default:
            break;
    }
}

static void update_hover(void) {
    layout_buttons();
    g_hoverBtn = -1;
    for (int i = 0; i < g_btnCount; i++)
        if (g_btns[i].enabled && point_in_rect(g_mouseX, g_mouseY, g_btns[i].r))
            g_hoverBtn = i;

    g_hoverTeam = -1;
    g_hoverPawn = -1;
    if (g_phase == PH_HUMAN_PICK_FREE || g_phase == PH_HUMAN_PICK_MOVE) {
        float best = 48.0f * 48.0f;
        for (int i = 0; i < 4; i++) {
            if (!is_selectable(g_turn, i)) continue;
            Vec3 w = cell_world(bidak[g_turn][i].x, bidak[g_turn][i].y);
            w.y += 0.38f;
            float sx, sy;
            if (!project_point(w, &sx, &sy)) continue;
            float dx = sx - g_mouseX, dy = sy - g_mouseY;
            float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; g_hoverTeam = g_turn; g_hoverPawn = i; }
        }
    }
}

static void process_click(void) {
    if (g_hoverBtn >= 0) { button_action(g_btns[g_hoverBtn].id); return; }
    if (g_hoverPawn >= 0) {
        if (g_phase == PH_HUMAN_PICK_FREE) do_release(g_turn, g_hoverPawn);
        else if (g_phase == PH_HUMAN_PICK_MOVE) do_move(g_turn, g_hoverPawn, g_roll);
    }
}

static void handle_key(SDL_Keycode k) {
    if (g_phase == PH_CONFIRM_EXIT) {
        if (k == SDLK_y) button_action(BTN_YES);
        else if (k == SDLK_n || k == SDLK_ESCAPE) button_action(BTN_NO);
        return;
    }
    if (k == SDLK_ESCAPE) {
        if (g_phase == PH_GAME_OVER) enter_phase(PH_MENU);
        else if (g_phase != PH_MENU) {
            g_confirmFrom = g_phase;
            enter_phase(PH_CONFIRM_EXIT);
        }
        return;
    }
    if (k == SDLK_r || k == SDLK_SPACE) {
        if (g_phase == PH_HUMAN_WAIT_ROLL) start_dice(0);
        return;
    }
    if (g_phase == PH_HUMAN_CHOICE) {
        if (k == SDLK_f && teams[g_turn]->insd > 0) {
            g_afterMove = AM_REROLL;
            enter_phase(PH_HUMAN_PICK_FREE);
        } else if (k == SDLK_m) {
            g_afterMove = AM_REROLL;
            enter_phase(PH_HUMAN_PICK_MOVE);
        }
        return;
    }
    if (k >= SDLK_1 && k <= SDLK_4) {
        int i = (int)(k - SDLK_1);
        if (!is_selectable(g_turn, i)) return;
        if (g_phase == PH_HUMAN_PICK_FREE) do_release(g_turn, i);
        else if (g_phase == PH_HUMAN_PICK_MOVE) do_move(g_turn, i, g_roll);
    }
}

static void handle_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            g_running = 0;
        } else if (e.type == SDL_KEYDOWN) {
            handle_key(e.key.keysym.sym);
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                g_click = 1;
                g_clickX = e.button.x;
                g_clickY = e.button.y;
                g_mouseX = e.button.x;
                g_mouseY = e.button.y;
            }
            if (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT) {
                g_drag = 1;
                g_lastMX = e.button.x;
                g_lastMY = e.button.y;
            }
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            g_drag = 0;
        } else if (e.type == SDL_MOUSEMOTION) {
            g_mouseX = e.motion.x;
            g_mouseY = e.motion.y;
            if (g_drag) {
                camYaw -= (float)(g_mouseX - g_lastMX) * 0.008f;
                camPitch += (float)(g_mouseY - g_lastMY) * 0.005f;
                if (camPitch < 0.30f) camPitch = 0.30f;
                if (camPitch > 1.35f) camPitch = 1.35f;
                g_lastMX = g_mouseX;
                g_lastMY = g_mouseY;
            }
        } else if (e.type == SDL_MOUSEWHEEL) {
            camDist -= (float)e.wheel.y * 1.3f;
            if (camDist < 9.0f) camDist = 9.0f;
            if (camDist > 34.0f) camDist = 34.0f;
        }
    }
}

/* =========================================================
 *  Frame
 * ========================================================= */
static void draw_frame(SDL_Renderer *r) {
    SDL_SetRenderDrawColor(r, 16, 18, 26, 255);
    SDL_RenderClear(r);

    render_scene_3d();
    flush3d(r);
    draw_selection_overlays(r);

    if (g_phase != PH_MENU) draw_hud(r);
    if (g_phase == PH_GAME_OVER) draw_gameover(r);
    if (g_phase == PH_MENU) draw_menu(r);
    if (g_phase == PH_CONFIRM_EXIT) draw_confirm(r);
    if (g_phase != PH_MENU) draw_banner(r);
    draw_buttons(r);
}

/* =========================================================
 *  Selftest
 * ========================================================= */
static int run_selftest(int games, int autoHuman) {
    int wins[4] = {0, 0, 0, 0};
    g_autoHuman = autoHuman;
    for (int g = 0; g < games; g++) {
        start_new_game();
        if (!autoHuman) g_player = -1;
        int steps = 0;
        while (g_phase != PH_GAME_OVER && steps < 400000) {
            adv(1.0 / 120.0);
            update_logic();
            steps++;
        }
        if (g_phase != PH_GAME_OVER) {
            printf("selftest: game %d TIMEOUT (phase %d, turn %d, player %d, roll %d,"
                   " inExtra %d, afterMove %d, deadline %.2f, now %.2f)\n",
                   g, g_phase, g_turn, g_player, g_roll, g_inExtra, g_afterMove,
                   g_deadline, g_now);
            for (int t = 0; t < 4; t++) {
                printf("  team %d: insd %d otsd %d selesai %d |", t, teams[t]->insd,
                       teams[t]->otsd, teams[t]->selesai);
                for (int i = 0; i < 4; i++)
                    printf(" p%d(in %d out %d tamat %d y %d x %d)", i,
                           teams[t]->inside[i], teams[t]->outside[i], bidak[t][i].tamat,
                           bidak[t][i].y, bidak[t][i].x);
                printf("\n");
            }
            g_autoHuman = 0;
            return 1;
        }
        if (g_winner < 0 || g_winner > 3) {
            printf("selftest: bad winner %d\n", g_winner);
            g_autoHuman = 0;
            return 1;
        }
        wins[g_winner]++;
    }
    g_autoHuman = 0;
    printf("selftest OK - %d games completed%s\n", games,
           autoHuman ? " (auto human)" : "");
    for (int i = 0; i < 4; i++) printf("  %-6s wins: %d\n", TEAM_NAMES[i], wins[i]);
    return 0;
}

/* =========================================================
 *  Main
 * ========================================================= */
int main(int argc, char *argv[]) {
    int selftest = 0;
    int selftestHuman = 0;
    int autoplay = 0;
    long maxFrames = 0;
    double speed = 1.0;
    double fixedDt = 0.0;
    int games = 200;
    const char *shotPath = NULL;
    long shotFrame = 60;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) {
            selftest = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') games = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--selftest-human")) {
            selftestHuman = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') games = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--autoplay")) {
            autoplay = 1;
        } else if (!strcmp(argv[i], "--frames")) {
            if (i + 1 < argc) maxFrames = atol(argv[++i]);
        } else if (!strcmp(argv[i], "--speed")) {
            if (i + 1 < argc) speed = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--fixed-dt")) {
            if (i + 1 < argc) fixedDt = atof(argv[++i]) / 1000.0;
        } else if (!strcmp(argv[i], "--screenshot")) {
            if (i + 1 < argc) shotPath = argv[++i];
        } else if (!strcmp(argv[i], "--shot-frame")) {
            if (i + 1 < argc) shotFrame = atol(argv[++i]);
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("LUDO 3D - SDL2 GUI remake of ludo.c\n");
            printf("usage: ludo-gui [--selftest [games]] [--selftest-human [games]]\n");
            printf("                [--autoplay] [--frames N] [--speed X]\n");
            printf("                [--screenshot FILE] [--shot-frame N]\n");
            return 0;
        }
    }

    srand((unsigned)time(NULL));
    g_light = vnorm(v3(-0.5f, 0.92f, 0.35f));

    if (selftest || selftestHuman) return run_selftest(games, selftestHuman);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("LUDO 3D",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       WIN_W, WIN_H,
                                       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                                       SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(ren, WIN_W, WIN_H);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    font_init();
    build_board();
    build_pawn();
    build_die();
    build_shadow();
    game_reset();
    if (autoplay) {
        start_new_game();
        g_player = -1;
    } else {
        enter_phase(PH_MENU);
    }

    long frames = 0;
    int shotDone = 0;
    Uint64 last = SDL_GetTicks64();
    while (g_running) {
        handle_events();
        Uint64 tick = SDL_GetTicks64();
        double dt = (double)(tick - last) / 1000.0;
        last = tick;
        if (fixedDt > 0) dt = fixedDt;
        if (dt > 0.1) dt = 0.1;
        dt *= speed;
        adv(dt);
        build_view_proj();
        update_hover();
        if (g_click) {
            process_click();
            g_click = 0;
        }
        update_logic();
        draw_frame(ren);
        SDL_RenderPresent(ren);
        frames++;
        if (shotPath && !shotDone &&
            (frames >= shotFrame ||
             (autoplay && g_phase == PH_GAME_OVER && g_now > g_phaseT + 1.5))) {
            shotDone = 1;
            int ow = 0, oh = 0;
            SDL_GetRendererOutputSize(ren, &ow, &oh);
            SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
            if (surf) {
                if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch) == 0)
                    SDL_SaveBMP(surf, shotPath);
                SDL_FreeSurface(surf);
            }
            g_running = 0;
        }
        if (maxFrames > 0 && frames >= maxFrames) g_running = 0;
        if (autoplay && g_phase == PH_GAME_OVER && g_now > g_phaseT + 3.0) g_running = 0;
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
