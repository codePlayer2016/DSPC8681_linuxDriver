KDIR=/usr/src/linux-headers-$(shell uname -r)
PRJ_HOME=$(shell pwd)
OUT_DIR = $(PRJ_HOME)/out
TEMP_DIR = $(PRJ_HOME)/out/temp
TARGET = DPU_driver_linux
obj-m = $(TARGET).o
$(TARGET)-objs = entry.o LinkLayer.o pcie.o crc32.o bootLoader.o common.o

all:
	rm -rf $(TEMP_DIR)
	mkdir  $(TEMP_DIR)
	cp -rf $(PRJ_HOME)/src/*.c $(TEMP_DIR)
	cp -rf $(PRJ_HOME)/inc/*.h $(TEMP_DIR)
	cp -rf $(PRJ_HOME)/Makefile $(TEMP_DIR)
	cd $(TEMP_DIR)
	$(MAKE) -C $(KDIR) M=$(TEMP_DIR)

	cp -rf $(TEMP_DIR)/*.ko $(OUT_DIR)
	rm -fr $(TEMP_DIR)
	
clean:
	rm -fr $(OUT_DIR)/*
	rm -fr $(TEMP_DIR)


