obj-m += hotspot.o
HOTSPOT_CFLAGS += -g -DDEBUG

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

debug:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
	EXTRA_CFLAGS="$(HOTSPOT_CFLAGS)"

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
