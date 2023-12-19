obj-m += kfetch_mod_109550132.o


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load: kfetch_mod_109550132.ko
	sudo insmod kfetch_mod_109550132.ko

unload: kfetch_mod_109550132.ko
	sudo rmmod kfetch_mod_109550132.ko

check:
	lsmod | grep kfetch
	cat /proc/modules | grep kfetch

show:
	sudo dmesg




