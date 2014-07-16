OBJECTS=src/memory.o src/parser.o src/stack.o src/utils.o src/libulver.o

all: libulver.a libulver.so
	$(CC) -I. -Wno-format -g -o ulver main.c libulver.a -lreadline

src/%.o: src/%.c
	$(CC) -I. -fPIC -Wno-format -g -o $@ -c $<

libulver.a: $(OBJECTS) 
	$(AR) rcs libulver.a $(OBJECTS)

libulver.so:
	$(CC) -shared -o libulver.so $(OBJECTS)

clean:
	rm -f src/*.o libulver.a libulver.so ulver

