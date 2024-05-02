/* 
*
* Purpose: Main driver for compiler.
* Author: Carly Retterer
* Date: 2 May 2024
*/ 

#include "llvm_parser.h"
#include "part1/semantic_analysis.h"

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s <c_file> <llvm_file>\n", argv[0]);
            return 1;
        }

        // Open the C file for Lex and Yacc parsing
        yyin = fopen(argv[1], "r");
        if (yyin == NULL) {
            fprintf(stderr, "File open error\n");
            return 1;
        }

    #ifdef YYDEBUG
        //yydebug = 1;
    #endif

        // Parse the C file
        yyparse();

        if (rootNode == NULL) {
            fprintf(stderr, "Root node is null\n");
            fclose(yyin);
            yylex_destroy();
            return 1;
        }

        // Call semantic analysis
        std::stack<SymbolTable> symbolTableStack;
        if (!visitNode(rootNode, symbolTableStack)) {
            fprintf(stderr, "Error: semantic analysis failed!\n");
            freeNode(rootNode);
            fclose(yyin);
            yylex_destroy();
            return 1;
        }

        freeNode(rootNode);
        fclose(yyin);
        yylex_destroy();

        LLVMModuleRef m = createLLVMModel(argv[2]);
        if (m != NULL) {
            LLVMDumpModule(m);
            walkFunctions(m);
            LLVMDumpModule(m);

            LLVMDisposeModule(m);
            LLVMShutdown();
        } else {
            fprintf("Failed to create module.\n");
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}