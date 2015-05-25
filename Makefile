run_qemu: build
	pebble install --qemu localhost:12344

run: build
	pebble install --phone 192.168.1.157 --logs
	
%.S.o: %.S
	$(shell dirname `which pebble`)/../arm-cs-tools/bin/arm-none-eabi-gcc -mthumb -mcpu=cortex-m3 -c -o $@ $<
	
build: src/asm.S.o src/nub-demo.c src/tunnel.c src/effect.h
	pebble build