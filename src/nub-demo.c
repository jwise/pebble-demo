#include <pebble.h>
#include "effect.h"

#define ASSERT(x) do { if (!(x)) { printf("assertion failed: " #x "\n"); *(volatile int *)0; } } while(0)

extern struct effect effect_tunnel;
extern struct effect effect_roto;

static struct effect *effect = &effect_roto;

static Window *s_main_window;
static Layer *s_window_layer;

/************************************ UI **************************************/

static time_t last_s = 0;
static uint16_t last_ms = 0;

uint32_t demotm = 0;
static uint32_t _tm20 = 0;
static uint32_t _frameno = 0;

#define FPS 60

static void poke(void *p) {
  time_t s;
  uint16_t ms;
  
  time_ms(&s, &ms);
  
  demotm += (s - last_s) * 1000;
  demotm += ((int)ms - (int)last_ms);
  
  last_s = s;
  last_ms = ms;
  
  _frameno++;
  if (_frameno == 20) {
    _tm20 = demotm;
  }
  
  if ((_frameno % 10) == 0 && _frameno > 20) {
    uint32_t frames = _frameno - 20;
    uint32_t frtm = demotm - _tm20;
    printf("%lu fps (%lu frames in %lu ms; %lu ms/frame)", frames * 1000 / frtm, frames, frtm, frtm / frames);
  }
  
  layer_mark_dirty(s_window_layer);
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
  
  effect->update(pxls, stride);
  
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
  
  effect->precalc();
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
