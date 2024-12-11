run: build
	time ./decoder logo.bmp

build:
	gcc -O0 -Wall -Wextra -Werror main.c -o decoder -lpthread
