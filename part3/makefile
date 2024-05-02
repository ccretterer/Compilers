LLVMCODE = llvm_parser
TEST = test

CXXFLAGS = -g3 -Wall -Wextra -pedantic -O0
CLANGFLAGS = -g -O0

$(LLVMCODE): $(LLVMCODE).c
	g++ $(CXXFLAGS) -I/usr/include/llvm-c-15/ -c $(LLVMCODE).c
	g++ $(LLVMCODE).o `llvm-config-15 --cxxflags --ldflags --libs core` -I/usr/include/llvm-c-15/ -o $@

llvm_file: $(TEST).c
	clang $(CLANGFLAGS) -S -emit-llvm $(TEST).c -o $(TEST).ll

test: llvm_file $(LLVMCODE)
	./$(LLVMCODE) $(TEST).ll

clean:
	rm -rf $(TEST).ll
	rm -rf $(LLVMCODE)
	rm -rf *.o
	rm -rf $(CFOLD).ll

valgrind: $(LLVMCODE)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$< $(TEST).ll
	