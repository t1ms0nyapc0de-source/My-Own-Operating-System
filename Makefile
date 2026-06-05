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
       src/vmm.o \
       src/timer.o \
       src/task.o \
       src/switch.o \
       src/vfs.o \
       src/tar.o \
       src/elf.o \
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

myos.iso: myos.bin initrd.tar
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp initrd.tar isodir/boot/initrd.tar
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

initrd.tar: src/test_program.c
	mkdir -p initrd
	$(CC) -c src/test_program.c -o src/test_program.o $(CFLAGS)
	$(LD) -Ttext 0x08048000 src/test_program.o -o initrd/hello $(LDFLAGS) -lgcc
	tar -cf initrd.tar -C initrd hello

run: myos.iso
	qemu-system-i386 -cdrom myos.iso -initrd initrd.tar

clean:
	rm -f src/*.o myos.bin myos.iso initrd.tar
	rm -rf isodir initrd