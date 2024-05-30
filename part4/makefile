# Define the LLVM configuration tool and flags
LLVM_CONFIG = llvm-config-15
LLVM_CFLAGS = `$(LLVM_CONFIG) --cflags`
LLVM_LDFLAGS = `$(LLVM_CONFIG) --ldflags --libs`

# Define the compiler and flags
CC = g++
CFLAGS = -Wall -g $(LLVM_CFLAGS)  # Added -g for debugging information

# Define the source files and the output executable name
C_SOURCES = semantic_analysis.c ast.c preprocessor.c llvm_builder.c llvm_parser.c
CPP_SOURCES = assembly_code_gen.cpp register_alloc.cpp main.cpp
LEXER = lex.l
PARSER = yacc.y
C_OBJECTS = $(C_SOURCES:.c=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp:.o)
LEXER_OBJECT = lex.yy.o
PARSER_OBJECT = yacc.tab.o
OBJECTS = $(C_OBJECTS) $(CPP_OBJECTS) $(LEXER_OBJECT) $(PARSER_OBJECT)
EXECUTABLE = compiler

# Default target
all: $(EXECUTABLE)

# Rule for building the executable
$(EXECUTABLE): $(OBJECTS)
	$(CC) -o $@ $^ $(LLVM_LDFLAGS)

# Rule for building object files from C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for building object files from C++ files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for building the parser
$(PARSER_OBJECT): yacc.y
	bison -d $(PARSER)
	$(CC) $(CFLAGS) -c yacc.tab.c -o $@

# Rule for building the lexer, ensure parser is built first
$(LEXER_OBJECT): lex.l yacc.tab.h
	flex -o lex.yy.c $(LEXER)
	$(CC) $(CFLAGS) -c lex.yy.c -o $@

# Ensure yacc.tab.h is built before lex.yy.c
yacc.tab.h: yacc.y
	bison -d yacc.y

# Run the program with Valgrind
valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXECUTABLE)

# Run the test with an input file
test: all
	./$(EXECUTABLE) p1.c

# Run the test with Valgrind and an input file
test-valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXECUTABLE) p1.c

# Clean up build artifacts, but not the source files
clean:
	rm -f $(EXECUTABLE) $(C_OBJECTS) $(CPP_OBJECTS) $(LEXER_OBJECT) $(PARSER_OBJECT) lex.yy.c yacc.tab.c yacc.tab.h
