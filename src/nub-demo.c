#include <pebble.h>

#define ASSERT(x) do { if (!(x)) { printf("assertion failed: " #x "\n"); *(volatile int *)0; } } while(0)

static Window *s_main_window;
static Layer *s_window_layer;

#define XRES 144
#define YRES 168

#define TEXSZ 256
#define DIST 6
/* PARTS: 0.5 */

#ifdef PBL_PLATFORM_APLITE
# define DIST_SCALE 2
#else
# define DIST_SCALE 1
#endif

uint8_t distmap[YRES / DIST_SCALE + 1][XRES / DIST_SCALE + 1];

/* fast isqrt32 algorithm from http://www.finesse.demon.co.uk/steven/sqrt.html */
#define iter1(N) \
    try = root + (1 << (N)); \
    if (n >= try << (N))   \
    {   n -= try << (N);   \
        root |= 2 << (N); \
    }

uint32_t isqrt (uint32_t n)
{
    uint32_t root = 0, try;
    iter1 (15);    iter1 (14);    iter1 (13);    iter1 (12);
    iter1 (11);    iter1 (10);    iter1 ( 9);    iter1 ( 8);
    iter1 ( 7);    iter1 ( 6);    iter1 ( 5);    iter1 ( 4);
    iter1 ( 3);    iter1 ( 2);    iter1 ( 1);    iter1 ( 0);
    return root >> 1;
}

static void precalc() {
  for (int y = 0; y < YRES / DIST_SCALE + 1; y++)
    for (int x = 0; x < XRES / DIST_SCALE + 1; x++) {
      uint32_t p = x * x * DIST_SCALE * DIST_SCALE + y * y * DIST_SCALE * DIST_SCALE;
      
      if (p == 0)
        p = 0xFF;
      else {
        p = TEXSZ * TEXSZ * DIST * DIST / p;
        p = isqrt(p);
        if (p > 0xFF)
          p = 0xFF;
      }
      
      distmap[y][x] = p; 
    }
}

/************************************ UI **************************************/

time_t last_s = 0;
uint16_t last_ms = 0;

uint32_t _tm = 0;
uint32_t _tm20 = 0;
uint32_t _frameno = 0;

#ifdef PBL_PLATFORM_APLITE
#  define LINEBUF_COMPONENTS 1
#else
#  define LINEBUF_COMPONENTS 3
#endif

/* make it two larger, to avoid the checks for running over on either side */
int16_t linebuf[2][XRES + 2][LINEBUF_COMPONENTS] = {{{0}}};

#define FPS 60

static void poke(void *p) {
  time_t s;
  uint16_t ms;
  
  time_ms(&s, &ms);
  
  _tm += (s - last_s) * 1000;
  _tm += ((int)ms - (int)last_ms);
  
  last_s = s;
  last_ms = ms;
  
  _frameno++;
  if (_frameno == 20) {
    _tm20 = _tm;
  }
  
  if ((_frameno % 10) == 0 && _frameno > 20) {
    uint32_t frames = _frameno - 20;
    uint32_t frtm = _tm - _tm20;
    printf("%lu fps (%lu frames in %lu ms; %lu ms/frame)", frames * 1000 / frtm, frames, frtm, frtm / frames);
  }
  
  layer_mark_dirty(s_window_layer);
}

/* atan2 lookup table algorithm from
 * http://www.coranac.com/documents/arctangent/.  Has a handful of the scale
 * coefficients baked in already at compile time.  */

#define BRAD_PI 0x80 /* Carefully chosen to make the texture size work out. */
#define LUTSZ 256

#define OCTANTIFY(_x, _y, _o)   do {                          \
  int _t; _o= 0;                                              \
  if(_y<  0)  {            _x= -_x;   _y= -_y; _o += 4; }     \
  if(_x<= 0)  { _t= _x;    _x=  _y;   _y= -_t; _o += 2; }     \
  if(_x<=_y)  { _t= _y-_x; _x= _x+_y; _y=  _t; _o += 1; }     \
} while(0);

const int32_t atan2_lut[] = {
#include "atan2lut.h"
};

static inline int32_t fastatan2(int32_t y, int32_t x) {
  if (y == 0) return x >= 0 ? 0 : BRAD_PI;
  
  /* First, enter an octant-normal form: although the output could
   * reasonably be in the range [0, 360°), everything is actually mirrored
   * around octants up to 45°.  So subtract out until we hit that, and then
   * write down the offset for what octant we actually started in.
   */
  int32_t octphi;
  OCTANTIFY(x, y, octphi);
  octphi *= BRAD_PI / 4;
  
  /* Now that we're in octant-normal form, we have the constraint that y < x
   * -- since the output, post-normalization, is on the range [0, 45°).  We
   * use a precomputed lookup table from y/x -- which, since y < x, is on
   * the range [0, 1) -- to an output that's on the range [0, 45°), or as we
   * might otherwise call it, [0, BRAD_PI/4).  We scale the input to the LUT
   * by the LUT size, which should be a power of two for efficiency.
   */
  int32_t lutphi = atan2_lut[y * LUTSZ / x];
  
  return octphi + lutphi;
}


static void update_proc(Layer *layer, GContext *ctx) {
  GBitmap *bm;
  uint8_t *pxls;
  
  app_timer_register(1, poke, NULL);
  
#ifdef PBL_PLATFORM_APLITE
  bm = graphics_capture_frame_buffer(ctx);
  int stride = gbitmap_get_bytes_per_row(bm);
#else
  bm = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
  const int stride = XRES;
#endif
  pxls = gbitmap_get_data(bm);
  
  uint8_t shiftx = TEXSZ * _tm * 7 / 40000;
  uint8_t shifty = TEXSZ * _tm * 3 / 40000;
  
  int32_t lookx = -XRES + XRES / 2 + XRES / 2 * sin_lookup(_tm * 7 / 2) * 8 / (10 * 0x10000);
  int32_t looky = -YRES + YRES / 2 + YRES / 2 * sin_lookup(_tm * 16 / 2) * 8 / (10 * 0x10000);
  
#ifdef PBL_PLATFORM_APLITE
  memset(pxls, 0, YRES * stride);
#endif
  
  uint8_t *linep = pxls;
  for (int y = 0; y < YRES; y++) {
    typedef int16_t fu_pixel[LINEBUF_COMPONENTS];

    fu_pixel *restrict errpxl = &(linebuf[y%2][1]);
    fu_pixel *restrict nxtpxl = &(linebuf[!(y % 2)][0]);

    for (int x = 0; x < 2; x++)
      for (int c = 0; c < LINEBUF_COMPONENTS; c++)
        nxtpxl[x][c] = 0;

#ifdef PBL_PLATFORM_APLITE
    extern void dither_aplite(uint8_t *linep, uint16_t *errpxl, uint16_t *nxtpxl, int32_t lookx, int32_t ylooky, uint32_t shiftx, uint32_t shifty, uint8_t *distmap, const int32_t *atan2_lut);
    dither_aplite(linep, (uint16_t *)errpxl[0], (uint16_t *)nxtpxl[0], lookx, y+looky, shiftx, shifty, &distmap[0][0], atan2_lut);
#else
    extern void dither_basalt(uint8_t *linep, uint16_t *errpxl, uint16_t *nxtpxl, int32_t lookx, int32_t ylooky, uint32_t shiftx, uint32_t shifty, uint8_t *distmap, const int32_t *atan2_lut);
    dither_basalt(linep, (uint16_t *)errpxl[0], (uint16_t *)nxtpxl[0], lookx, y+looky, shiftx, shifty, &distmap[0][0], atan2_lut);
#endif
    linep += stride;
  }
  
  graphics_release_frame_buffer(ctx, bm);
}

static void window_load(Window *window) {
  s_window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(s_window_layer);
  
  ASSERT(window_bounds.size.w == XRES);
  ASSERT(window_bounds.size.h == YRES);

  layer_set_update_proc(s_window_layer, update_proc);
}

static void window_unload(Window *window) {
}

/*********************************** App **************************************/

static void init() {
  srand(time(NULL));
  
  precalc();
  time_ms(&last_s, &last_ms);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
