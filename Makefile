

all: image.iso

image.iso: multiboot/grub.cfg kernel/kernel.elf multiboot/multiboot.elf
	mkdir -p iso/boot/grub
	cp multiboot/grub.cfg iso/boot/grub
	cp multiboot/multiboot.elf iso/boot
	cp kernel/kernel.elf iso/boot
	grub-mkrescue -o image.iso iso/

kernel/kernel.elf:
	make -C kernel
	
multiboot/multiboot.elf:
	make -C multiboot

simu: image.iso
	qemu-system-x86_64 -device isa-debug-exit,iobase=0xf4,iosize=0x04 -drive file=image.iso,format=raw -debugcon stdio -s

simugdb: image.iso
	qemu-system-x86_64 -device isa-debug-exit,iobase=0xf4,iosize=0x04 -drive file=image.iso,format=raw -debugcon stdio -s -S

clean:
	rm -rf *.o image.iso iso
	make -C multiboot clean
	make -C kernel clean

#cp grub.cfg iso/boot/grub/ && grub-mkrescue -o image.iso iso/

