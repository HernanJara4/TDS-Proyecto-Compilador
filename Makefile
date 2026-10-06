# =====================================================================
# Makefile del compilador C-TDS - Taller de Diseño de Software (UNRC)
#
#   make            compila build/c-tds (flex + bison + gcc)
#   make test       corre los tests publicos que tienen .expected
#   make clean      borra los archivos generados en build/
#
# El orden importa: bison genera build/Gramatica.tab.h con los codigos
# de los tokens, y el lexer (Gramatica.l) necesita ese header para
# compilar, por eso lex.yy.c depende de el.
# =====================================================================

CC      = gcc
CFLAGS  = -Wall -Ibuild -Iinclude
BISON   = bison
FLEX    = flex

BUILD   = build
SRC     = src
INC     = include

TARGET  = $(BUILD)/c-tds

PARSER_C = $(BUILD)/Gramatica.tab.c
PARSER_H = $(BUILD)/Gramatica.tab.h
LEXER_C  = $(BUILD)/lex.yy.c

SOURCES  = $(SRC)/ast/ast.c \
           $(SRC)/semantic/tabla.c \
           $(SRC)/semantic/typecheck.c

HEADERS  = $(INC)/ast.h $(INC)/tabla.h $(INC)/semantica.h

.PHONY: all clean test

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(PARSER_C): $(SRC)/parser/Gramatica.y | $(BUILD)
	$(BISON) -d -o $@ $<

# bison -d escribe ambos archivos; esta regla solo cubre el caso en que
# falte el header (lo vuelve a generar junto con el .c).
$(PARSER_H): $(SRC)/parser/Gramatica.y | $(BUILD)
	$(BISON) -d -o $(PARSER_C) $<

$(LEXER_C): $(SRC)/scanner/Gramatica.l $(PARSER_H) | $(BUILD)
	$(FLEX) -o $@ $<

$(TARGET): $(PARSER_C) $(LEXER_C) $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(PARSER_C) $(LEXER_C) $(SOURCES)

test: $(TARGET)
	./tests/run_tests.sh

clean:
	rm -f $(PARSER_C) $(PARSER_H) $(LEXER_C) $(TARGET)
