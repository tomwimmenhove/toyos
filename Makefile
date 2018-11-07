all:
	make -C kernel
	make -C multiboot
	make image.iso

image.iso:
	mkdir -p iso/boot/grub
	cp multiboot/grub.cfg iso/boot/grub
	cp multiboot/multiboot.elf iso/boot
	cp kernel/kernel.elf iso/boot
	grub-mkrescue -o image.iso iso/

simu: all
	qemu-system-x86_64 -enable-kvm -device isa-debug-exit,iobase=0xf4,iosize=0x04 -drive file=image.iso,format=raw -debugcon stdio -s -monitor unix:qemu.mon,server,nowait

simugdb: all
	qemu-system-x86_64 -enable-kvm -device isa-debug-exit,iobase=0xf4,iosize=0x04 -drive file=image.iso,format=raw -debugcon stdio -s -monitor unix:qemu.mon,server,nowait -S

monitor:
	minicom -D unix\#qemu.mon

clean:
	rm -rf *.o image.iso iso
	make -C multiboot clean
	make -C kernel clean

