run: build
	time ./decoder logo.bmp

cave: build
	time ./decoder cave.bmp

build:
	gcc -O0 -Wall -Wextra -Werror main.c -o decoder -lpthread
