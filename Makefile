EXEC = g-v2

CC = gcc
CFLAGS = -Wall -Wextra -Wno-unused-function -std=c11 -D_POSIX_C_SOURCE=200809L

LEX = g-v2.l
YACC = g-v2.y

LEX_OUT = lex.yy.c
YACC_OUT = g-v2.tab.c
YACC_HDR = g-v2.tab.h

OBJS = lex.yy.o g-v2.tab.o ast.o symtab.o semantic.o codagen.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS) -lfl

$(YACC_OUT) $(YACC_HDR): $(YACC)
	bison -d $(YACC)

$(LEX_OUT): $(LEX) $(YACC_HDR)
	flex $(LEX)

lex.yy.o: $(LEX_OUT) $(YACC_HDR)
	$(CC) $(CFLAGS) -c $(LEX_OUT)

g-v2.tab.o: $(YACC_OUT) $(YACC_HDR) ast.h semantic.h codagen.h
	$(CC) $(CFLAGS) -c $(YACC_OUT)

ast.o: ast.c ast.h
	$(CC) $(CFLAGS) -c ast.c

symtab.o: symtab.c symtab.h ast.h
	$(CC) $(CFLAGS) -c symtab.c

semantic.o: semantic.c semantic.h symtab.h ast.h $(YACC_HDR)
	$(CC) $(CFLAGS) -c semantic.c

codagen.o: codagen.c codagen.h ast.h $(YACC_HDR)
	$(CC) $(CFLAGS) -c codagen.c

clean:
	rm -f $(EXEC) *.o $(LEX_OUT) $(YACC_OUT) $(YACC_HDR)
