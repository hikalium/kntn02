PROG = grpwk
OBJS = template.o main.o
CC = gcc
CFLAGS = -O2 -Wall

NUM = 1000
TESTDATA = testmaker/$(NUM)


.SUFFIXES: .c

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $^
.c.o:
	$(CC) $(CFLAGS) -c $<

chk/chk:
	make -C chk

clean:
	rm  $(OBJS) $(PROG)

$(TESTDATA)_ref:
	cd testmaker; make NUM=$(NUM)

test: grpwk chk/chk $(TESTDATA)_ref $(TESTDATA)_in
	./grpwk $(TESTDATA)_ref < $(TESTDATA)_in > out.txt
	head -1 $(TESTDATA)_in > base.txt
	./chk/chk base.txt $(TESTDATA)_ref
	./chk/chk out.txt $(TESTDATA)_ref
	
