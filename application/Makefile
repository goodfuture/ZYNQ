include common.mk 

OBJS = main.o csr.o app.o inifile.o lfsr.o gps.o

$(IMAGE):$(OBJS)
	$(CC) $(OBJS) -lm -lpthread -o $(IMAGE)
main.o: main.c main.h common.h csr.h
	$(CC) $(CFLAGS) -c main.c 
csr.o: csr.c common.h csr.h
	$(CC) $(CFLAGS) -c csr.c
app.o: app.c app.h common.h csr.h
	$(CC) $(CFLAGS) -c app.c app.h
inifil.o: inifile.c inifile.h 
	$(CC) $(CFLAGS) -c inifile.c
lfsr.o: lfsr.c lfsr.h 
	$(CC) $(CFLAGS) -c lfsr.c
gps.o: gps.c gps.h 
	$(CC) $(CFLAGS) -c gps.c

.PHONY: clean
clean:
	rm -fv $(IMAGE) *.o
