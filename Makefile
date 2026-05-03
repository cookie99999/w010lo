AS = m68k-elf-as
LD = m68k-elf-ld
CC = m68k-elf-gcc
OBJCOPY = m68k-elf-objcopy
CCFLAGS = -mcpu=68000 -march=68000 -Wall -Os -ffreestanding
LDFLAGS = -mcpu=68000 -march=68000 -T link.ld -nostdlib -nostartfiles

all: upper.bin lower.bin

upper.bin: monitor.bin
	$(OBJCOPY) --interleave=2 --byte=0 -I binary -O binary $< $@

lower.bin: monitor.bin
	$(OBJCOPY) --interleave=2 --byte=1 -I binary -O binary $< $@

monitor.bin: monitor.s
	vasmm68k_mot -x -spaces -Fbin -L monitor.lst -o $@ $<

c:
	$(CC) $(CCFLAGS) -c ctest.c -o ctest.o
	$(CC) $(LDFLAGS) ctest.o -lgcc -o ctest.elf
	$(OBJCOPY) -S -O binary ctest.elf ctest.bin

clean:
	rm -rf *.lst *.o *.elf *.bin
