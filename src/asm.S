.syntax unified
.code 16

#define XRES 144
#define YRES 168

@ Update me if pushed registers change
#define STACKSZ 36

@ Update me if parameters change
#define YLOOKY (STACKSZ+4)
#define SHIFTX (STACKSZ+8)
#define SHIFTY (STACKSZ+12)
#define DISTMAP (STACKSZ+16)
#define ATANLUT (STACKSZ+20)


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Macros
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ DIST:
@   input:
@     r8: x
@     r10: lookx
@     r11: y+looky
@     [sp, #DISTMAP]: distmap
@     [sp, #SHIFTY]: shifty
@   output:
@     r2: falloff
@     r7: shiftx + dist -- unsaturated
@   clobbers: r0, r3
.macro DIST scale
	adds r0, r8, r10             @ r0 S32 = x + lookx
	it lt                        @ r0 < 0?
	rsblt r0, r0, #0             @   r0 S32 = abs(x + lookx)
.if \scale == 2
	mov r0, r0, lsr #1
.endif

	cmp r11, #0                     @ We have to multiply through
	ite lt                          @ eventually, and we have to load
	movlt r2, #-(XRES / \scale + 1) @ a size anyhow, so may as well bake
	movge r2, #(XRES / \scale + 1)  @ in the absolute value for y+looky...

.if \scale == 2
	mov r3, r11, asr #1
	mla r0, r3, r2, r0         @ r0 = offset into distance table
.else
	mla r0, r11, r2, r0         @ r0 = offset into distance table
.endif
	ldr r1, [sp, #DISTMAP]
	ldrb r0, [r1, r0]           @ r0 U8 = dist
	
	rsbs r2, r0, #128           @ r3 U8 = 128 - dist
	it lt
	movlt r2, #0                @ r3 saturate 0
	
	ldr r7, [sp, #SHIFTX]       @ r7 = shiftx
	add r7, r0                  @ r7 = shiftx + dist (unsaturated)
.endm

@ ATAN:
@   input:
@     r8: x
@     r10: lookx
@     r11: y+looky
@     [sp, #ATANLUT]: atan2_lut
@     [sp, #SHIFTY]: shifty
@   output:
@     r6: (256 - atanl) + shifty -- unsaturated
@   clobbers: r0, r1, r3, r4, r5
.macro ATAN	
	add r0, r8, r10             @ r0 = _x = x + lookx
	adds r1, r11, #0            @ r1 = _y = y + looky
	
	ittte lt                    @ _y < 0?
	rsblt r0, r0, #0            @   _x = -_x
	rsblt r1, r1, #0            @   _y = -_y
	movlt r3, #128              @   _o = 4*BRAD_PI/4
	movge r3, #0                @ else _o = 0
	cmp r0, #0                  @ _x <= 0?
	itttt le
	movle r4, r0                @   _t = _x
	movle r0, r1                @   _x = _y
	rsble r1, r4, #0            @   _x = -_t
	addle r3, r3, #64           @   _o += 2*BRAD_PI/4
	cmp r0, r1                  @ _x <= _y?
	ittt le
	@ goal: _x = _x + _y and _y = _y - _x
	suble r1, r1, r0            @   _y = _y - _x
	addle r0, r1, r0, lsl #1    @   _x = _y + _x * 2
	addle r3, r3, #32           @   _o += 1*BRAD_PI/4
	
	mov r1, r1, lsl #8          @ _y *= LUTSZ
	sdiv r1, r0                 @ _y /= x
	ldr r0, [sp, #ATANLUT]      @ r0 = atan2_lut
	ldr r5, [sp, #SHIFTY]       @ r5 = shifty
	ldrb r0, [r0, r1, lsl #2]   @ r0 = atan2_lut[_y / _x]
	add r6, r0, r3              @ r6 = atanl = octphi + lutphi
	
	rsb r6, r6, #256
	add r6, r6, r5              @ r6 = (256 - atanl) + shifty (unsaturated)
.endm

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Main routines
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.align 3
@ tunnel_basalt:
@   Calculates one line at a time, in 3-component color.
@
@   r0: framebuffer (bytep)
@   r1: linebuf errpxl
@   r2: linebuf nxtpxl
@   r3: lookx
@   sp+4: y+looky
@   sp+8: shiftx
@   sp+12: shifty
@   sp+16: distmap
@   sp+20: atan2_lut
.globl tunnel_basalt
tunnel_basalt:
	push {r4-r12,r14} @ stack: 10 words = 40 bytes

	mov r9, r0	    @ r9 = framebuffer
	mov r10, r3         @ r10 = lookx
	ldr r11, [sp, #YLOOKY] @ r11 = y+looky
	mov r12, r1         @ r12 = linebuf errpxl
	mov r14, r2         @ r14 = linebuf nxtpxl

	mov r8, #0          @ r8 = reps
	
1:	
	@@@ distance computation @@@
	@ live: r8: x
	@       r9: framebuffer
	@       r10: lookx
	@       r11: y+looky
	@       r12: linebuf errpxl
	@       r14: linebuf nexpxl


	DIST 1 @ (r8, r10, r11) -> (r2, r7), x r0 r3

	@@@ angle computation @@@
	@ live: r2: falloff
	@       r7: coordx U32
	@       r8: x
	@       r9: framebuffer
	@       r10: lookx
	@       r11: y+looky
	@       r12: linebuf errpxl
	@       r14: linebuf nexpxl

	ATAN @ (r8, r10, r11) -> r6, x r0 r1 r2 r3 r4 r5
	
	@@@ pixel computation @@@
	eor r1, r7, r6              @ r1 = pixel = coordx + coordy
	uxtb r1, r1                 @ r1 U8

@ DITHER:
@   input: r12: errpxl
@          r14: nxtpxl
@          r3: pixel value
@          r2: falloff
@  output: appropriately placed bit in r0
@ clobber: r3, r4, r5, r6, r7
@
@    args: #1: component

.macro DITHER c
	mul r3, r2, r3               @ r3 U.7 = falloff(pxl)
	ldrsh r4, [r12, #0+\c*2]     @ r4 S.4 = errpxl[0]
	adds r3, r3, r4, lsl #3      @ r3 S.7 = want = falloff(pxl) + errpxl
	ite lt                       @ want < 0?
.if \c == 0
	movlt r0, #0                 @   T: r8 U2 = dither = 0
	usatge r0, #2, r3, asr #13   @   F: r8 U2 = dither = sat(want >> (7 + 6))
	mov r5, #0x55
	mul r5, r5, r0               @ r5 U8 = dither * 0x55
.else
	movlt r4, #0                 @   T: r4 U2 = dither = 0
	usatge r4, #2, r3, asr #13   @   F: r4 U2 = dither = sat(want >> (7 + 6))
	orr r0, r0, r4, lsl #\c*2
	mov r5, #0x55
	mul r5, r5, r4               @ r5 U8 = dither * 0x55
.endif
	rsb r3, r5, r3, asr #7       @ r3 U8 = err = want - dither * 0x55
	
	ldrh r4, [r12, #6+\c*2]      @ r4 = errpxl[1]
	ldrh r5, [r14, #0+\c*2]      @ r5 = nxtpxl[0]
	ldrh r6, [r14, #6+\c*2]      @ r6 = nxtpxl[1]
	mov r7, #7
	mla r4, r3, r7, r4           @ r4 = errpxl[1] + err * 7
	mov r7, #3
	mla r5, r3, r7, r5           @ r5 = nxtpxl[0] + err * 3
	mov r7, #5
	mla r6, r3, r7, r6           @ r6 = nxtpxl[1] + err * 5
	strh r4, [r12, #6+\c*2]
	strh r5, [r14, #0+\c*2]
	strh r6, [r14, #6+\c*2]
	strh r3, [r14, #12+\c*2]     @ nxtpxl[2] = err
.endm

#ifdef RAW_DBG
	@ RAW_DBG mode just blasts r1 dithered into all three channels
	mov r3, r1
	mov r2, #128
	DITHER 0
	orr r0, r0, r0, lsl #2
	orr r0, r0, r0, lsl #2
#else
	orr r1, r1, r1, lsl #8      @ extend pixel to replicate
	ubfx r3, r1, #8, #8
	DITHER 0
	
	ubfx r3, r1, #7, #8
	DITHER 1
	
	ubfx r3, r1, #6, #8
	DITHER 2
#endif
	strb r0, [r9, r8]
	
	add r12, r12, #6
	add r14, r14, #6
	
	add r8, r8, #1
	cmp r8, #XRES
	bne 1b	
	pop {r4-r12,r14}
	bx lr

.align 3
@ tunnel_aplite:
@   Calculates one line at a time, in 1-bit color.
@
@   r0: framebuffer (bytep)
@   r1: linebuf errpxl
@   r2: linebuf nxtpxl
@   r3: lookx
@   sp+4: y+looky
@   sp+8: shiftx
@   sp+12: shifty
@   sp+16: distmap
@   sp+20: atan2_lut
.globl tunnel_aplite
tunnel_aplite:
	push {r4-r12,r14} @ stack: 10 words = 40 bytes

	mov r9, r0	    @ r9 = framebuffer
	mov r10, r3         @ r10 = lookx
	ldr r11, [sp, #YLOOKY] @ r11 = y+looky
	mov r12, r1         @ r12 = linebuf errpxl
	mov r14, r2         @ r14 = linebuf nxtpxl

	mov r8, #0          @ r8 = reps
	
1:	
	@@@ distance computation @@@
	@ live: r8: x
	@       r9: framebuffer
	@       r10: lookx
	@       r11: y+looky
	@       r12: linebuf errpxl
	@       r14: linebuf nexpxl

	DIST 2 @ (r8, r10, r11) -> (r2, r7), x r0

	@@@ angle computation @@@
	@ live: r2: falloff
	@       r7: coordx U32
	@       r8: x
	@       r9: framebuffer
	@       r10: lookx
	@       r11: y+looky
	@       r12: linebuf errpxl
	@       r14: linebuf nexpxl

	ATAN @ (r8, r10, r11) -> r6, x r0 r1 r2 r3 r4 r5
	
	@@@ pixel computation @@@
	eor r3, r7, r6              @ r3 = pixel = coordx + coordy
	uxtb r3, r3                 @ r3 U8

	@ live: r2: falloff
	@       r1: pixel value
	@       r8: x
	@       r9: framebuffer
	@       r10: lookx
	@       r11: y+looky
	@       r12: linebuf errpxl
	@       r14: linebuf nexpxl

	mul r3, r2, r3               @ r3 U.7 = falloff(pxl)
	ldrsh r4, [r12, #0]          @ r4 S.4 = errpxl[0]
	adds r3, r3, r4, lsl #3      @ r3 S.7 = want = falloff(pxl) + errpxl
	cmp r3, #0x80
	ite ge
	movge r0, #1
	movlt r0, #0
	mov r5, #0xFF
	mul r5, r5, r0               @ r5 U8 = dither * 0xFF
	rsb r3, r5, r3, asr #7       @ r3 U8 = err = want - dither * 0xFF
	
	ldrh r4, [r12, #2]           @ r4 = errpxl[1]
	ldrh r5, [r14, #0]           @ r5 = nxtpxl[0]
	ldrh r6, [r14, #2]           @ r6 = nxtpxl[1]
	mov r7, #7
	mla r4, r3, r7, r4           @ r4 = errpxl[1] + err * 7
	mov r7, #3
	mla r5, r3, r7, r5           @ r5 = nxtpxl[0] + err * 3
	mov r7, #5
	mla r6, r3, r7, r6           @ r6 = nxtpxl[1] + err * 5
	strh r4, [r12, #2]
	strh r5, [r14, #0]
	strh r6, [r14, #2]
	strh r3, [r14, #4]           @ nxtpxl[2] = err

	ands r2, r8, #7
	lsl r0, r0, r2
	mov r3, r8, asr #3
	itee eq
	moveq r1, r0
	ldrbne r1, [r9, r3]
	orrne r1, r0
	strb r1, [r9, r3]
	
	add r12, r12, #2
	add r14, r14, #2
	
	add r8, r8, #1
	cmp r8, #XRES
	bne 1b
	pop {r4-r12,r14}
	bx lr


