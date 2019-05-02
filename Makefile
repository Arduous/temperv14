.PHONY: clean rules-install
all:	temperusb

CFLAGS = -O2 -Wall

temperusb:	temperv14.c
	${CC} -o $@ $^ -lusb-1.0 

clean:		
	rm -f temperv14 *.o

rules-install:			# must be superuser to do this
	cp 99-tempsensor.rules /etc/udev/rules.d
