run: build
	./decoder logo.bmp

cave: build
	./decoder cave.bmp

build:
	gcc -O0 -Wall -Wextra -Werror main.c -o decoder -lpthread

perf:
	gcc -O0 -Wall -Wextra -Werror -pg main.c -o decoder -lpthread
	./decoder cave.bmp
	mv gmon.out gmon.sum
	./decoder cave.bmp
	gprof -s decoder gmon.out gmon.sum
	./decoder cave.bmp
	gprof -s decoder gmon.out gmon.sum
	gprof decoder gmon.sum > profile.txt
