CC =		gcc
CFLAGS =	-Wall -O2 \
		`libjulius-config --cflags` `libsent-config --cflags`
LDLIBS =	`libjulius-config --libs` `libsent-config --libs`

all: jremocon

check: jremocon
	./jremocon --verbose

clean:
	-rm *.o jremocon

jremocon: jremocon.o
