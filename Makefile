CC = i686-elf-gcc
AS = nasm
LD = i686-elf-gcc

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -ffreestanding -O2 -nostdlib

all: myos.iso

src/boot.o: src/boot.s
	$(AS) -f elf32 src/boot.s -o src/boot.o

src/kernel.o: src/kernel.c
	$(CC) -c src/kernel.c -o src/kernel.o $(CFLAGS)

myos.bin: src/boot.o src/kernel.o
	$(LD) -T src/linker.ld -o myos.bin $(LDFLAGS) -lgcc src/boot.o src/kernel.o

myos.iso: myos.bin
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

run:
	qemu-system-i386 -cdrom myos.iso

clean:
	rm -f src/*.o myos.bin myos.iso
	rm -rf isodir