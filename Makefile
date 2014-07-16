all:
	$(CC) -I. -Wno-format -g -o ulver main.c src/utils.c src/stack.c src/memory.c src/parser.c src/libulver.c
