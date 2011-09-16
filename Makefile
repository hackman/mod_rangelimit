BUILD_DIR=/usr/local/apache2/build/
APXS=/usr/local/apache2/bin/apxs

all:
	sudo $(APXS) -cia mod_rangelimit.c

clean:
	sudo rm -rf .libs *.o *.lo *.so *.la *.a *.slo


