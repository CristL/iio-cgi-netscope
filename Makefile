DESTDIR=/usr/local
#CC=gcc
EXEC = ndso
OBJS = ndso.o cgivars.o htmllib.o iio.o int_fft.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS) -lm

$(EXEC2): $(OBJS2)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS2) $(LDLIBS)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

$(OBJS): cgivars.h htmllib.h

install:
	install -d $(DESTDIR)/bin
	install -d /var/www
	install ./thttpd_init.sh $(DESTDIR)/bin/
	chmod +x $(DESTDIR)/bin/thttpd_init.sh
	cp -a ./www/* /var/www/
	install ndso /var/www/data/cgi-bin/ndso.cgi
