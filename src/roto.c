/* We start off bounding x and y on the screen space:
 *
 *  x \in [0, XRES)
 *  y \in [0, YRES)
 *
 * We wish to rotate around the center, so we start with a translation into
 * the middle, and then we'll translate back later.
 *
 *   x -= XRES / 2
 *   y -= YRES / 2
 *
 * We can now rotate around the center.
 *
 *   x' = x * cos(k1*t) + y * -sin(k1*t)
 *   y' = x * sin(k1*t) + y * cos(k1*t)
 *
 * And scale:
 *
 *   x' *= k3 * sin(k2*t) + k4
 *   y' *= k3 * sin(k2*t) + k4
 *
 * And finally translate back into the texture offset that we want:
 *
 *   x' += k5*t
 *   y' += k5*t
 *
 * Moving everything through,
 *
 *   (f1, f2, f3, f4 constant per frame)
 *   f1 = k5*t
 *   f2 = k3 * sin(k2*t) + k4
 *   f3 = cos(k1*t)
 *   f4 = sin(k1*t)
 *
 *   x' = k5*t + (k3 * sin(k2*t) + k4) * ((x - XRES / 2) * cos(k1*t) - (y - YRES / 2) * sin(k1*t))
 *   x' = f1 + f2 * ((x - XRES / 2) * f3 - (y - YRES / 2) * f4)
 *   x' = f1 + f2 * f3 * x - f2 * f3 * XRES / 2 - f2 * f4 * y + f2 * f4 * YRES / 2
 *
 *   x'(0, 0) = f1 - f2 * f3 * XRES / 2 + f2 * f4 * YRES / 2
 *   dx'/dx = f2 * f3
 *   dx'/dy = -f2 * f4
 *
 * and, analogously,
 *
 *   y'(0, 0) = f1 - f2 * f4 * XRES / 2 - f2 * f3 * YRES / 2
 *   dy'/dx = f2 * f4
 *   dy'/dy = f2 * f3
 *
 */

#include <pebble.h>
#include "effect.h"

#ifdef PBL_PLATFORM_APLITE
#  define LINEBUF_COMPONENTS 1
#else
#  define LINEBUF_COMPONENTS 3
#endif

/* make it two larger, to avoid the checks for running over on either side */
static int16_t linebuf[2][XRES + 2][LINEBUF_COMPONENTS] = {{{0}}};

static void precalc() {
}


/* We've carefully scaled everything so that, since the inputs to the roto
 * kernel routine are U16.16, we can just multiply through by the trig
 * functions.  We need to shift to normalize when multiplying two trig
 * functions together, though.  */


static void update(uint8_t *pxls, int stride) {
  /* Angular speeds when multiplied by time are such that 65.5 is about 1Hz. */
  const uint32_t texofs /* f1 */ = demotm / 4;
  const uint32_t scale /* f2; U16.16 */ = sin_lookup(20 * demotm) * 2 / 3 + 0x10000;
  const uint32_t f2f3 /* f2*f3; U16.16 */ = (cos_lookup(10 * demotm) * (scale >> 8)) >> 8;
  const uint32_t f2f4 /* f2*f4; U16.16 */ = (sin_lookup(10 * demotm) * (scale >> 8)) >> 8;
  
  uint32_t xp = texofs * 0x10000 - f2f3 * XRES / 2 + f2f4 * YRES / 2;
  uint32_t yp = texofs * 0x8000 - f2f4 * XRES / 2 - f2f3 * YRES / 2;
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
    extern void roto_aplite(uint32_t xp0y, uint32_t yp0y, uint8_t *linep, uint32_t dxpdx, uint32_t dypdx, uint16_t *errpxl, uint16_t *nxtpxl);
    roto_aplite(xp, yp, linep, f2f3, f2f4, (uint16_t *)errpxl[0], (uint16_t *)nxtpxl[0]);
#else
    extern void roto_basalt(uint32_t xp0y, uint32_t yp0y, uint8_t *linep, uint32_t dxpdx, uint32_t dypdx, uint16_t *errpxl, uint16_t *nxtpxl);
    roto_basalt(xp, yp, linep, f2f3, f2f4, (uint16_t *)errpxl[0], (uint16_t *)nxtpxl[0]);
#endif

    xp += -f2f4;
    yp += f2f3;
    
    linep += stride;
  }
}

struct effect effect_roto = { precalc, update };
