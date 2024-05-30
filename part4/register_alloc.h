/*
*   Purpose: This is my .h file for register allocation. 
*   Author: Carly Retterer
*   Date: 30 May 2024
*/

#ifndef REGISTER_ALLOCATION_H
#define REGISTER_ALLOCATION_H

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>

#define NUM_REGISTERS 3

// Function declarations
std::pair<std::map<LLVMValueRef, int>, std::map<LLVMValueRef, std::pair<int, int>>> compute_liveness(LLVMBasicBlockRef BB);
LLVMValueRef find_spill(LLVMValueRef Instr, std::map<LLVMValueRef, int>& reg_map, std::map<LLVMValueRef, int>& inst_index, std::vector<LLVMValueRef>& sorted_list, std::map<LLVMValueRef, std::pair<int, int>>& live_range);
void registerAllocation(LLVMModuleRef module);

#endif // REGISTER_ALLOCATION_H
