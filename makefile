source = miniC

# for memory-leak tests
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all

$(source).out: $(source).l $(source).y semantic_analysis.c
	yacc -v -d --debug $(source).y
	lex $(source).l
	g++ -g -o $(source).out lex.yy.c y.tab.c ast.c semantic_analysis.c


clean:
	rm -f lex.yy.c y.tab.c y.tab.h $(source).out vgcore.*

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
	$(VALGRIND) ./$(source).out < parser_tests/p1.c
	$(VALGRIND) ./$(source).out < parser_tests/p2.c
	$(VALGRIND) ./$(source).out < parser_tests/p3.c
	$(VALGRIND) ./$(source).out < parser_tests/p4.c
	$(VALGRIND) ./$(source).out < parser_tests/p5.c
