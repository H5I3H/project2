TARGET = rs232_slave
obj-m = $(TARGET).o
KDIR = /lib/modules/`uname -r`/build

all: module

module:
	make -C $(KDIR) M=`pwd` modules

clean:
	make -C $(KDIR) M=`pwd` clean