OBJECTS=src/memory.o src/parser.o src/stack.o src/utils.o src/libulver.o
ifeq ($(OS), Windows_NT)
	LDFLAGS=
	LIBS=
	CFLAGS=
	LIBNAME=ulver.dll
	BINNAME=ulver.exe
	TEST=ulver_tests.exe
else
	LDFLAGS=-rdynamic -ldl
	LIBS=-lreadline
	CFLAGS=-fPIC
	LIBNAME=libulver.so
	BINNAME=ulver
	TEST=ulver_tests
endif

all: libulver.a $(LIBNAME)
	$(CC) -I. -g -o $(BINNAME) main.c libulver.a $(LDFLAGS) $(LIBS)

src/%.o: src/%.c
	$(CC) -I. $(CFLAGS) -g -o $@ -c $<

libulver.a: $(OBJECTS) 
	$(AR) rcs libulver.a $(OBJECTS)

$(LIBNAME):
	$(CC) -shared -o $(LIBNAME) $(OBJECTS) $(LDFLAGS)

test: libulver.a
	@$(CC) -I. -g -o $(TEST) t/tests.c libulver.a $(LDFLAGS)
	@./ulver_tests
	@rm $(TEST)


clean:
	rm -f src/*.o libulver.a $(LIBNAME) $(BINNAME)

