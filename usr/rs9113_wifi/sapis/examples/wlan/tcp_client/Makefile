# Make File

rm=/bin/rm -f
CC= cc
DEFS=  
PROGNAME= rsi_wc_app
LINKS= -lpthread -L../../../build/ -lrps


RSI_DRIVER_PATH = ../../../include

CFLAGS=  -I$(RSI_DRIVER_PATH)/
CFLAGS+= -D RSI_SAMPLE_HAL

TCP_CLIENT_APP = rsi_tcp_client.o 

all: $(TCP_CLIENT_APP) 

	make wlan CC=$(CC) -C ../../../build/ 
	$(CC) $(CFLAGS) *.o $(LINKS) -o $(PROGNAME) 
	rm -f  *.o  


clean:
	$(rm) $(TCP_CLIENT_APP) core *.o *~ rsi_wc_app
