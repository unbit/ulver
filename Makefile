OBJECTS=src/memory.o src/parser.o src/stack.o src/utils.o src/funcs_thread.o src/funcs_math.o src/funcs_seq.o src/funcs_string.o src/funcs_hashtable.o src/funcs_io.o src/funcs_coro.o src/coro.o src/hub.o src/libulver.o
ifeq ($(OS), Windows_NT)
	LDFLAGS=
	LIBS=-luv -ladvapi32 -liphlpapi -lpsapi -lshell32 -lws2_32
	CFLAGS=-D_WIN32_WINNT=0x0600
	ifneq ($(UV), "")
		CFLAGS+=-I$(UV)/include
		LDFLAGS+=-L$(UV)/
	endif
	ifneq ("$(wildcard libuv/include/uv.h)","")
		CFLAGS+=-Ilibuv/include
		LDFLAGS+=-Llibuv/
	endif
	LIBNAME=ulver.dll
	BINNAME=ulver.exe
	TEST=ulver_tests.exe
else
	OS=$(shell uname)
	LDFLAGS=-rdynamic
	LIBS=-ldl -lpthread -lreadline
	CFLAGS=-fPIC
	ifeq ($(OS), Darwin)
		CFLAGS+=-D_XOPEN_SOURCE -Wno-deprecated-declarations
	endif	
	ifeq ($(SPLITSTACK), 1)
		CFLAGS+=-fuse-ld=gold -fsplit-stack -DSPLITSTACK
	endif
	ifneq ("$(wildcard libuv/include/uv.h)","")
		CFLAGS+=-Ilibuv/include
		LDFLAGS+=libuv/.libs/libuv.a
	else
		LIBS += -luv
	endif
	LIBNAME=libulver.so
	BINNAME=ulver
	TEST=ulver_tests
endif

all: libulver.a $(LIBNAME)
	$(CC) $(CFLAGS) -Iinclude -g -o $(BINNAME) main.c libulver.a $(LDFLAGS) $(LIBS)

src/%.o: src/%.c include/ulver.h
	$(CC) -Iinclude $(CFLAGS) -g -o $@ -c $<

libulver.a: $(OBJECTS) 
	$(AR) rcs libulver.a $(OBJECTS)

$(LIBNAME):
	$(CC) -shared -o $(LIBNAME) $(OBJECTS) $(LDFLAGS) $(LIBS)

test: libulver.a
	@$(CC) $(CFLAGS) -Iinclude -g -o $(TEST) t/tests.c libulver.a $(LDFLAGS) $(LIBS)
	@./ulver_tests
	@rm $(TEST)

clean:
	rm -f src/*.o libulver.a $(LIBNAME) $(BINNAME)

