OBJECTS=src/memory.o src/parser.o src/stack.o src/utils.o src/libulver.o
ifeq ($(OS), Windows_NT)
	LIBS=
	CFLAGS=
	TEST=ulver_tests.exe
else
	LIBS=-lreadline
	CFLAGS=-fPIC
	TEST=ulver_test
endif

all: libulver.a libulver.so
	$(CC) -I. -g -o ulver main.c libulver.a $(LIBS)

src/%.o: src/%.c
	$(CC) -I. $(CFLAGS) -g -o $@ -c $<

libulver.a: $(OBJECTS) 
	$(AR) rcs libulver.a $(OBJECTS)

libulver.so:
	$(CC) -shared -o libulver.so $(OBJECTS)

test: libulver.a
	@$(CC) -I. -g -o ulver_tests t/tests.c libulver.a
	@./ulver_tests
	@rm $(TEST)


clean:
	rm -f src/*.o libulver.a libulver.so ulver

