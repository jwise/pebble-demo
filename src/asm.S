.syntax unified
.code 16

#define XRES 144

.align 3
@ dither_basalt:
@   r0: framebuffer (bytep)
@   r1: linebuf errpxl
@   r2: linebuf nxtpxl
@   r3: falloff (bytep)
.globl dither_basalt
dither_basalt:
	push {r4-r12,r14}
	
	mov r10, r0	@ r10 = framebuffer
	mov r11, r3	@ r11 = falloff
	
	mov r0, #0	@ r0 = reps
	
	@ r3: pixel value
	@ r5: falloff
.1:	

@ DITHER:
@   input: r1: errpxl
@          r2: nxtpxl
@          r3: pixel value
@          r14: falloff
@  output: appropriately placed bit in r9
@ clobber: r3, r4, r5, r6, r7
@
@    args: #1: component

.macro DITHER c
	mul r3, r3, r14              @ r3 U.7 = falloff(pxl)
	ldrsh r4, [r1, #0+\c*2]      @ r4 S.4 = errpxl[0]
	adds r3, r3, r4, lsl #3      @ r3 S.7 = want = falloff(pxl) + errpxl
	ite lt                       @ want < 0?
.if \c == 0
	movlt r9, #0                 @   T: r9 U2 = dither = 0
	usatge r9, #2, r3, asr #13   @   F: r9 U2 = dither = sat(want >> (7 + 6))
	mov r5, #0x55
	mul r5, r5, r9               @ r5 U8 = dither * 0x55
.else
	movlt r4, #0                 @   T: r4 U2 = dither = 0
	usatge r4, #2, r3, asr #13   @   F: r4 U2 = dither = sat(want >> (7 + 6))
	orr r9, r9, r4, lsl #\c*2
	mov r5, #0x55
	mul r5, r5, r4               @ r5 U8 = dither * 0x55
.endif
	rsb r3, r5, r3, asr #7       @ r3 U8 = err = want - dither * 0x55
	
	ldrh r4, [r1, #6+\c*2]       @ r4 = errpxl[1]
	ldrh r5, [r2, #0+\c*2]       @ r5 = nxtpxl[0]
	ldrh r6, [r2, #6+\c*2]       @ r6 = nxtpxl[1]
	mov r7, #7
	mla r4, r3, r7, r4           @ r4 = errpxl[1] + err * 7
	mov r7, #3
	mla r5, r3, r7, r5           @ r5 = nxtpxl[0] + err * 3
	mov r7, #5
	mla r6, r3, r7, r6           @ r6 = nxtpxl[1] + err * 5
	strh r4, [r1, #6+\c*2]
	strh r5, [r2, #0+\c*2]
	strh r6, [r2, #6+\c*2]
	strh r3, [r2, #12+\c*2]      @ nxtpxl[2] = err
.endm

	ldrb r12, [r10, r0]
	ldrb r14, [r11, r0]
	add r12, r12, r12, lsl #8
	
	ubfx r3, r12, #8, #8
	DITHER 0
	
	ubfx r3, r12, #7, #8
	DITHER 1
	
	ubfx r3, r12, #6, #8
	DITHER 2

	strb r9, [r10, r0]
	
	add r1, r1, #6
	add r2, r2, #6
	
	add r0, r0, #1
	cmp r0, #XRES
	bne .1	
	pop {r4-r12,r14}
	bx lr
