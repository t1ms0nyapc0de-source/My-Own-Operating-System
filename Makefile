CC = i686-elf-gcc
AS = nasm
LD = i686-elf-gcc

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Isrc
LDFLAGS = -ffreestanding -O2 -nostdlib

OBJS = src/boot.o \
       src/gdt_flush.o \
       src/isr.o \
       src/user_switch.o \
       src/string.o \
       src/gdt.o \
       src/idt.o \
       src/syscall.o \
       src/user_mode.o \
       src/pmm.o \
       src/kernel.o

all: myos.iso

# Assembly pattern rule
%.o: %.s
	$(AS) -f elf32 $< -o $@

# C compilation pattern rule
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

myos.bin: $(OBJS)
	$(LD) -T src/linker.ld -o myos.bin $(LDFLAGS) -lgcc $(OBJS)

myos.iso: myos.bin
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

run: myos.iso
	qemu-system-i386 -cdrom myos.iso

clean:
	rm -f src/*.o myos.bin myos.iso
	rm -rf isodir