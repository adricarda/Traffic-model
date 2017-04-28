ALL: traffic

CFLAGS += -std=c99 -Wall -pedantic -O3

traffic: traffic.c

movie: traffic
	./traffic 1024 0.4 && avconv -i "out%05d.ppm" -vcodec mpeg4 -s 512x512 traffic.avi

clean:
	\rm -f *.ppm traffic *.o *~ traffic.avi
