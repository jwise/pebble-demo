#include <pebble.h>

#define ASSERT(x) do { if (!(x)) { printf("assertion failed: " #x "\n"); *(volatile int *)0; } } while(0)

static Window *s_main_window;
static Layer *s_window_layer;

#define XRES 144
#define YRES 168

#define TEXSZ 256
#define DIST 6
/* PARTS: 0.5 */

uint8_t distmap[YRES / 2 + 1][XRES / 2 + 1];

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
  for (int y = 0; y < YRES / 2 + 1; y++)
    for (int x = 0; x < XRES / 2 + 1; x++) {
      uint32_t p = x * x * 4 + y * y * 4;
      
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
  
  if ((_frameno % 10) == 0) {
    printf("%lu fps (%lu frames in %lu ms; %lu ms/frame)", _frameno * 1000 / _tm, _frameno, _tm, _tm / _frameno);
  }
  
  layer_mark_dirty(s_window_layer);
}

static void update_proc(Layer *layer, GContext *ctx) {
  GBitmap *bm;
  uint8_t *pxls;
  
  app_timer_register(1000 / FPS, poke, NULL);
  
#ifdef PBL_PLATFORM_APLITE
  bm = graphics_capture_frame_buffer(ctx);
  int stride = gbitmap_get_bytes_per_row(bm);
#else
  bm = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
#endif
  pxls = gbitmap_get_data(bm);
  
  uint8_t shiftx = TEXSZ * _tm * 7 / 40000;
  uint8_t shifty = TEXSZ * _tm * 3 / 40000;
  
  int32_t lookx = XRES / 2 + XRES / 2 * sin_lookup(_tm * 7 / 2) * 8 / (10 * 0x10000);
  int32_t looky = YRES / 2 + YRES / 2 * sin_lookup(_tm * 16 / 2) * 8 / (10 * 0x10000);
  
  for (int y = 0; y < YRES; y++) {
    for (int x = 0; x < XRES + 2; x++)
      for (int c = 0; c < LINEBUF_COMPONENTS; c++)
        linebuf[(y + 1) % 2][x][c] = 0;
#ifdef PBL_PLATFORM_APLITE
    for (int x = 0; x < XRES / 8; x++)
      pxls[y * stride + x] = 0;
#endif
      
    for (int x = 0; x < XRES; x++) {
      int32_t x_wrap;
      int32_t y_wrap;
      
      /* distance */
      x_wrap = x + lookx;
      y_wrap = y + looky;
      
      x_wrap -= XRES; if (x_wrap < 0) x_wrap = -x_wrap;
      y_wrap -= YRES; if (y_wrap < 0) y_wrap = -y_wrap;
      
#ifndef ACCURATE_DISTANCE
      uint8_t dist = distmap[y_wrap / 2][x_wrap / 2];
#else
      uint16_t _dist;
      if (x_wrap & 1) {
        if (y_wrap & 1) {
          _dist = distmap[y_wrap / 2][x_wrap / 2];
          _dist += distmap[y_wrap / 2 + 1][x_wrap / 2];
          _dist += distmap[y_wrap / 2 + 1][x_wrap / 2 + 1];
          _dist += distmap[y_wrap / 2][x_wrap / 2];
          _dist += 2;
          _dist >>= 2;
        } else {
          _dist = distmap[y_wrap / 2][x_wrap / 2];
          _dist += distmap[y_wrap / 2][x_wrap / 2 + 1];
          _dist += 1;
          _dist >>= 1;
        }
      } else {
        if (y_wrap & 1) {
          _dist = distmap[y_wrap / 2][x_wrap / 2];
          _dist += distmap[y_wrap / 2 + 1][x_wrap / 2];
          _dist += 1;
          _dist >>= 1;
        } else
          _dist = distmap[y_wrap / 2][x_wrap / 2];
      }
      uint8_t dist = _dist;
#endif

      uint8_t falloff = dist / 48;
      if (falloff < 1)
        falloff = 1;
      
      uint8_t coordx = dist + shiftx;
      
      /* angle */
      int32_t atanl = atan2_lookup(x + lookx - XRES, y + looky - YRES);
      atanl >>= 8; /* * TEXSZ / 2 / 0x8000 */
      
      uint8_t coordy = ((256 - atanl) & 255) + shifty;
      
#ifdef PBL_PLATFORM_APLITE
      uint8_t pxl = (coordx ^ coordy) / falloff;
      
      int16_t want = pxl + linebuf[y & 1][x+1][0] / 16;
      if (want < 0)
        want = 0;
      if (want > 0xFF)
        want = 0xFF;
      
      int dither = want >= 0x80;
      int err = want - (dither ? 0xFF : 0x00);
      
      linebuf[  y&1 ][x+2][0] += err * 7;
      linebuf[!(y&1)][x  ][0] += err * 3;
      linebuf[!(y&1)][x+1][0] += err * 5;
      linebuf[!(y&1)][x+2][0] += err;
      
      pxls[y * stride + x / 8] |= dither << (x % 8);
#else
      /* texture */
      uint8_t tex_r = (((coordx ^ coordy) * 0x101) >> 6) / falloff;
      uint8_t tex_g = (((coordx ^ coordy) * 0x101) >> 7) / falloff;
      uint8_t tex_b = (((coordx ^ coordy) * 0x101) >> 8) / falloff;
      
      pxls[y*XRES+x] = ((tex_r >> 6) << 4) |
                       ((tex_g >> 6) << 2) |
                       ((tex_b >> 6) << 0);
#endif
    }
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
