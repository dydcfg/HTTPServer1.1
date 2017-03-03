CC=gcc
CFLAGS=-I.
DEPS = parse.h y.tab.h log.h
OBJ = y.tab.o lex.yy.o parse.o log.o liso.o
FLAGS = -g -Wall

default:all

all: liso

lex.yy.c: lexer.l
	flex $^

y.tab.c: parser.y
	yacc -d $^

%.o: %.c $(DEPS)
	$(CC) $(FLAGS) -c -o $@ $< $(CFLAGS)

liso: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *~ *.o liso lex.yy.c y.tab.c y.tab.h
