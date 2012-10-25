CC=/usr/bin/gcc-4.6
CFLAGS=-m32 -shared -fPIC -Wall -Wno-deprecated -g
CXX=/usr/bin/g++-4.6
CXXFLAGS=-m32 -shared -fPIC -Wall -O2 -Wno-deprecated -g
LD=/usr/bin/ld

OBJ = adom-sage.o library.o states.o command.o options.o io.o msg_handlers.o \
	spells.o #loader.o starsign.o roller.o item_list.o autosave.o
TARGET = adom-sage.so

all: $(TARGET) adom-sage

clean:
	/bin/rm -f $(TARGET) $(OBJ) adom-sage config.h

adom-sage: sage-frontend.cc
	$(CXX) -m32 -o adom-sage sage-frontend.cc -lncurses -g

# HACK: Gratuitously link ncurses to adom-sage, then run ldd on adom-sage to
# find its path.  Adom-sage will try running ldd on adom, so this code is
# hopefully redundant.
config.h: adom-sage
	ldd adom-sage | perl -ne '($$lib, $$path) = (/^\s+(libncurses|libc).*=>\s+(\S+)/) and $$lib =~ tr/a-z/A-Z/ and print "#define $$lib \"$$path\"\n"' >config.h
	echo '#define LIBNCURSES "/lib/i386-linux-gnu/libncurses.so.5"' >> config.h
	@grep LIBNCURSES config.h > /dev/null || \
		(echo Unable to find libncurses && exit 1)
	@grep LIBC config.h > /dev/null || \
		(echo Unable to find libc && exit 1)

adom-sage.so: $(OBJ)
	$(CXX) -m32 -shared -o adom-sage.so $(OBJ) -ldl
#	$(CXX) -Wl,-Bshareable -shared -fPIC -o adom-sage.so $(OBJ) -ldl

library.o : library.cc config.h
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

watcher.so: watcher.c
	$(CC) $(CFLAGS) -c -o watcher.o watcher.c
	$(LD) -Bshareable -o watcher.so watcher.o -ldl

