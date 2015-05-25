#ifndef _EFFECT_H
#define _EFFECT_H

#define XRES 144
#define YRES 168

extern uint32_t demotm;

struct effect {
	void (*precalc)();
	void (*update)(uint8_t *pxls, int stride);
};

#endif
