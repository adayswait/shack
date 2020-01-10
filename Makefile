OUTPUT_NAME = repl
CC          = gcc
CFLAGS      = -DWITH_MAIN -I. -O2 -g -ldl -lm -Wl,-export-dynamic

FILES       = shack.c shack.h
repl: $(FILES)
	$(CC) -o $(OUTPUT_NAME) $(FILES) $(CFLAGS) 