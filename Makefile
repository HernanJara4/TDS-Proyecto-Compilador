CC      = gcc
CFLAGS  = -Wall -Wextra -I build/ -I include/
FLEX    = flex
BISON   = bison

BUILD_DIR = build
TARGET    = $(BUILD_DIR)/c-tds

LEX_SRC   = src/scanner/Gramatica.l
YACC_SRC  = src/parser/Gramatica.y

LEX_GEN   = $(BUILD_DIR)/lex.yy.c
YACC_GEN  = $(BUILD_DIR)/Gramatica.tab.c
YACC_HDR  = $(BUILD_DIR)/Gramatica.tab.h

C_SRCS    = src/main.c \
            src/ast/ast.c \
            src/semantic/tabla.c \
            src/semantic/typecheck.c \
            src/ir/ir.c \
            src/asm/codegen.c \
            src/opt/optimizer.c

HDRS      = include/ast.h \
            include/tabla.h \
            include/typecheck.h \
            include/ir.h \
            include/codegen.h \
            include/optimizer.h

.PHONY: all clean test test-parser test-semantic test-ir test-asm test-opt

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(YACC_GEN) $(YACC_HDR): $(YACC_SRC) | $(BUILD_DIR)
	$(BISON) -d -o $(YACC_GEN) $(YACC_SRC)

$(LEX_GEN): $(LEX_SRC) $(YACC_HDR) | $(BUILD_DIR)
	$(FLEX) -o $(LEX_GEN) $(LEX_SRC)

$(TARGET): $(YACC_GEN) $(LEX_GEN) $(C_SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(YACC_GEN) $(LEX_GEN) $(C_SRCS)

test-parser: $(TARGET)
	@for f in tests/01_scanner_parser/public/*.txt; do \
		echo "=== $$f ==="; \
		./$(TARGET) -target parse "$$f" || true; \
		echo ""; \
	done

test-semantic: $(TARGET)
	@for f in tests/02_semantic/public/*.txt; do \
		echo "=== $$f ==="; \
		./$(TARGET) -target sem -debug "$$f" || true; \
		echo ""; \
	done

test-ir: $(TARGET)
	@for f in tests/03_ir/public/*.txt; do \
		echo "=== $$f ==="; \
		./$(TARGET) -target codinter -debug "$$f" || true; \
		echo ""; \
	done

test-asm: $(TARGET)
	@for f in tests/04_asm/public/*.txt; do \
		echo "=== $$f ==="; \
		./$(TARGET) -target assembly -debug -o $(BUILD_DIR)/test_bin "$$f"; \
		./$(BUILD_DIR)/test_bin; \
		echo "-> Codigo de retorno al ejecutar el binario x86-64: $$?"; \
		rm -f $(BUILD_DIR)/test_bin $(BUILD_DIR)/test_bin.s; \
		echo ""; \
	done

test-opt: $(TARGET)
	@for f in tests/05_opt/public/*.txt; do \
		echo "=== $$f ==="; \
		./$(TARGET) -opt all -debug -o $(BUILD_DIR)/test_opt_bin "$$f"; \
		./$(BUILD_DIR)/test_opt_bin; \
		echo "-> Codigo de retorno del binario optimizado: $$?"; \
		rm -f $(BUILD_DIR)/test_opt_bin; \
		echo ""; \
	done

test: test-parser test-semantic test-ir test-asm test-opt

clean:
	rm -rf $(BUILD_DIR)/*
