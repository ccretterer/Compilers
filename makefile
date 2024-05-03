source = miniC
TEST = test

CXXFLAGS = -g3 -Wall -Wextra -pedantic -O0
CLANGFLAGS = -g -O0

$(source).out: $(source).l $(source).y semantic_analysis.c part3/llvm_parser.c
	yacc -v -d --debug $(source).y
	lex $(source).l
	g++ $(CXXFLAGS) -I/usr/include/llvm-c-15/ -c part3/llvm_parser.c -o part3/llvm_parser.o
	g++ -g -o $(source).out lex.yy.c y.tab.c ast.c semantic_analysis.c part3/llvm_parser.o `llvm-config-15 --cxxflags --ldflags --libs core` -I/usr/include/llvm-c-15/

llvm_file: $(TEST).c
	clang $(CLANGFLAGS) -S -emit-llvm $(TEST).c -o $(TEST).ll

run: $(source).out llvm_file
	./$(source).out $(TEST).c $(TEST).ll

clean:
	rm -f lex.yy.c y.tab.c y.tab.h $(source).out vgcore.* $(TEST).ll part3/*.o

# Test cases
test1: $(source).out
	./$(source).out < parser_tests/p1.c

test2: $(source).out
	./$(source).out < parser_tests/p2.c

test3: $(source).out
	./$(source).out < parser_tests/p3.c

test4: $(source).out
	./$(source).out < parser_tests/p4.c

test5: $(source).out
	./$(source).out < parser_tests/p5.c

badtest: $(source).out
	./$(source).out < parser_tests/p_bad.c

# Valgrind
valgrind: $(source).out
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(source).out $(TEST).c $(TEST).ll