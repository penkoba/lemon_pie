CC =		gcc
CFLAGS =	-Wall -W -Wno-unused-parameter -O2 \
		`libjulius-config --cflags` `libsent-config --cflags`
LDLIBS =	`libjulius-config --libs` `libsent-config --libs`

all: lemon_pie

check: lemon_pie
	./lemon_pie --verbose

clean:
	-rm *.o lemon_pie

lemon_pie.o: lemon_pie.c sensor_cmd.h
lemon_pie: lemon_pie.o
