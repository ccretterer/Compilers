#include "semantic_analysis.h"
#include "ast.h"
#include "preprocessor.h"
#include <cstdio>
#include <stack>
#include <vector>
#include "llvm_builder.h"
#include "llvm_parser.h"  // Include the llvm_parser header

extern "C" {
    #include <llvm-c/Core.h>
    #include <llvm-c/Analysis.h>
    #include <llvm-c/BitWriter.h>
    #include <llvm-c/Initialization.h>
}

extern FILE *yyin;             // Declare yyin as an external variable
extern int yyparse();          // Declare yyparse as an external function
extern void yylex_destroy();   // Declare yylex_destroy as an external function
extern astNode *rootNode;      // Declare rootNode as an external variable

using SymbolTable = std::vector<char *>;

LLVMModuleRef generateLLVMIR(astNode* root);
LLVMValueRef functionTraversal(LLVMModuleRef mod, astNode* funcNode); // Declare the function here
void rename_variables(astNode* node);

// Function declarations for register allocation and assembly generation
void registerAllocation(LLVMModuleRef module);
void generateAssembly(LLVMModuleRef module);

int main(int argc, char* argv[]) {
    if (argc == 2) {
        yyin = fopen(argv[1], "r");
        if (yyin == NULL) {
            fprintf(stderr, "File open error\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    #ifdef YYDEBUG
    yydebug = 1;
    #endif

    yyparse();

    if (rootNode == NULL) {
        fprintf(stderr, "root is null\n");
        return 1;
    }

    std::stack<SymbolTable> symbolTableStack;
    if (rootNode != NULL) {
        if (!visitNode(rootNode, symbolTableStack)) {
            fprintf(stderr, "didn't visit root node\n");
            freeNode(rootNode);
            return 1;
        }

        // Preprocess to rename variables
        rename_variables(rootNode);

        // Generate LLVM IR
        LLVMModuleRef mod = generateLLVMIR(rootNode);

        // Optionally, you can print the generated LLVM IR to stdout
        char* ir_string = LLVMPrintModuleToString(mod);
        printf("%s", ir_string);
        LLVMDisposeMessage(ir_string);

        // Write LLVM IR to a file
        if (LLVMPrintModuleToFile(mod, "output.ll", nullptr) != 0) {
            fprintf(stderr, "Error writing LLVM IR to file\n");
        }

        // Call the llvm_parser function to perform optimizations
        walkFunctions(mod);

        // Perform register allocation
        registerAllocation(mod);

        // Generate assembly code
        generateAssembly(mod);

        // Cleanup the module
        LLVMDisposeModule(mod);

        freeNode(rootNode);
    }  

    fclose(yyin);
    yylex_destroy();
    LLVMShutdown(); // Clean up LLVM's internal state

    return 0;
}

// Function to generate LLVM IR from AST
LLVMModuleRef generateLLVMIR(astNode* root) {
    LLVMModuleRef mod = LLVMModuleCreateWithName("my_module");
    LLVMSetTarget(mod, "x86_64-pc-linux-gnu");

    LLVMBuilderRef builder = LLVMCreateBuilder();

    // Generate extern function declarations for print and read
    LLVMTypeRef int32Type = LLVMInt32Type();
    LLVMTypeRef voidType = LLVMVoidType();
    LLVMTypeRef printType = LLVMFunctionType(voidType, &int32Type, 1, 0);
    LLVMAddFunction(mod, "print", printType);
    
    LLVMTypeRef readType = LLVMFunctionType(int32Type, NULL, 0, 0);
    LLVMAddFunction(mod, "read", readType);

    // Visit the function node (assuming root is a function node)
    LLVMValueRef func = functionTraversal(mod, root);

    // Memory cleanup
    LLVMDisposeBuilder(builder);

    return mod;
}
