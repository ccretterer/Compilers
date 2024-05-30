/*
*   Purpose:  This is my .h file for assembly code generation. 
*   Author: Carly Retterer
*   Date: 30 May 2024
*/

#ifndef GENERATE_ASSEMBLY_H
#define GENERATE_ASSEMBLY_H

#include <iostream>
#include <vector>
#include <map>
#include <cstdarg>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>

// Function declarations
void createBBLabels(LLVMModuleRef module, std::map<LLVMBasicBlockRef, std::string>& bb_labels);
void printDirectives(LLVMValueRef function);
void printFunctionEnd();
void getOffsetMap(LLVMModuleRef module, LLVMValueRef function, int& localMem, std::map<LLVMValueRef, int>& offset_map);
void emit(const char *format, ...);

void generateAssembly(LLVMModuleRef module);

#endif // GENERATE_ASSEMBLY_H
