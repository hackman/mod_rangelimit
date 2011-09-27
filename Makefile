APXS13=/usr/local/apache/bin/apxs
APXS22=/usr/local/apache2/bin/apxs

ap13:
	sudo $(APXS13) -cia mod_rangelimit.c

ap22:
	sudo $(APXS22) -cia mod_rangelimit.c

all:
	ap13

clean:
	sudo rm -rf .libs *.o *.lo *.so *.la *.a *.slo


