CC=g++
LDFLAGS=-lx264 -lswscale -lavutil -lavformat -lavcodec
SRC=src/$(shell ls src/)

all: compile clean

compile:
	$(CC) $(SRC) -o bin/linux_x86_64.out $(LDFLAGS)

clean: 
	echo "clean"