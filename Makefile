CC =		gcc
CFLAGS =	-Wall -W -Wno-unused-parameter -O2 \
		`libjulius-config --cflags` `libsent-config --cflags`
LDLIBS =	`libjulius-config --libs` `libsent-config --libs`

all: jremocon

check: jremocon
	./jremocon --verbose

clean:
	-rm *.o jremocon

jremocon: jremocon.o
