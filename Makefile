obj-m += pps_timer.o
obj-m += pps_gpio.o

KDIR :=/usr/src/linux-headers-`uname -r`/
PWD_DIR := `pwd`

all:
	make -C $(KDIR) M=$(PWD_DIR) modules

clean:
	rm -rf *.ko *.o *.mod.o *.mod.c *.symvers *.cmd .tmp* modules.order .*.cmd .*.mk *.mod
