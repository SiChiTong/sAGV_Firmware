# Make File

rm=/bin/rm -f
CC= cc
DEFS=  
PROGNAME= rsi_wc_app
LINKS= -lpthread -L../../../build/ -lrps


RSI_DRIVER_PATH = ../../../include

CFLAGS=  -I$(RSI_DRIVER_PATH)/
CFLAGS+= -D RSI_SAMPLE_HAL

AP_START_APP = rsi_ap_start.o 

all: $(AP_START_APP) 

	make wlan CC=$(CC) -C ../../../build/ 
	$(CC) $(CFLAGS) *.o $(LINKS) -o $(PROGNAME) 
	rm -f  *.o  


clean:
	$(rm) $(AP_START_APP) core *.o *~ rsi_wc_app
