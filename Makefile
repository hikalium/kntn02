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
clean:
	rm  $(OBJS) $(PROG)

test:
	cd testmaker; make NUM=$(NUM)
	./grpwk < $(TESTDATA)_in > out.txt
	head -1 $(TESTDATA)_in > base.txt
	./chk/chk base.txt $(TESTDATA)_ref
	./chk/chk out.txt $(TESTDATA)_ref
	
