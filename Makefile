OBJECTS=src/memory.o src/parser.o src/stack.o src/utils.o src/io.o src/coro.o src/hub.o src/libulver.o
ifeq ($(OS), Windows_NT)
	LDFLAGS=-luv
	LIBS=
	CFLAGS=
	LIBNAME=ulver.dll
	BINNAME=ulver.exe
	TEST=ulver_tests.exe
else
	LDFLAGS=-luv -rdynamic -ldl -lpthread
	LIBS=-lreadline
	CFLAGS=-fPIC -D_XOPEN_SOURCE -Wno-deprecated-declarations
	LIBNAME=libulver.so
	BINNAME=ulver
	TEST=ulver_tests
endif

ifeq ($(NO_SPLITSTACK), 1)
	CFLAGS+= -DNO_SPLITSTACK
endif

all: libulver.a $(LIBNAME)
	$(CC) -fuse-ld=gold -I. -g -o $(BINNAME) main.c libulver.a $(LDFLAGS) $(LIBS)

src/%.o: src/%.c ulver.h
	$(CC) -fsplit-stack -I. $(CFLAGS) -g -o $@ -c $<

libulver.a: $(OBJECTS) 
	$(AR) rcs libulver.a $(OBJECTS)

$(LIBNAME):
	$(CC) -fuse-ld=gold -shared -o $(LIBNAME) $(OBJECTS) $(LDFLAGS)

test: libulver.a
	@$(CC) -I. -g -o $(TEST) t/tests.c libulver.a $(LDFLAGS)
	@./ulver_tests
	@rm $(TEST)


clean:
	rm -f src/*.o libulver.a $(LIBNAME) $(BINNAME)

