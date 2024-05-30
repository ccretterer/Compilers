/*
*   Purpose: This file is a responsible for handing the register allocation process for the backend processing of the compiler. 
*   It follows the algorithm provided on Canvas and is based on the linear scan register allocation algorithm.
*   Author: Carly Retterer
*   Date:  30 May 2024
*
*
*/

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

std::pair<std::map<LLVMValueRef, int>, std::map<LLVMValueRef, std::pair<int, int>>> compute_liveness(LLVMBasicBlockRef BB) {
    std::map<LLVMValueRef, int> inst_index;
    std::map<LLVMValueRef, std::pair<int, int>> live_range;
    
    int index = 0;
    // First pass: assign indices to each instruction
    for (LLVMValueRef Instr = LLVMGetFirstInstruction(BB); Instr; Instr = LLVMGetNextInstruction(Instr)) {
        if (LLVMGetInstructionOpcode(Instr) == LLVMAlloca) {
            continue; // Ignore alloc instructions
        }
        inst_index[Instr] = index++;
        live_range[Instr] = {inst_index[Instr], inst_index[Instr]};
    }

    // Second pass: update the live ranges based on operands usage
    for (LLVMValueRef Instr = LLVMGetFirstInstruction(BB); Instr; Instr = LLVMGetNextInstruction(Instr)) {
        if (LLVMGetInstructionOpcode(Instr) == LLVMAlloca) {
            continue; // Ignore alloc instructions
        }
        for (unsigned i = 0; i < LLVMGetNumOperands(Instr); ++i) {
            LLVMValueRef operand = LLVMGetOperand(Instr, i);
            if (inst_index.find(operand) != inst_index.end()) {
                live_range[operand].second = std::max(live_range[operand].second, inst_index[Instr]);
            }
        }
    }

    return {inst_index, live_range};
}

// sorted list is by the end of live range in descending order
LLVMValueRef find_spill(LLVMValueRef Instr, std::map<LLVMValueRef, int>& reg_map, std::map<LLVMValueRef, int>& inst_index, std::vector<LLVMValueRef>& sorted_list, std::map<LLVMValueRef, std::pair<int, int>>& live_range) {
    for (LLVMValueRef V : sorted_list) {
        // checking that current variable is still live and that it's currently assigned to a register
        if (live_range[V].second > inst_index[Instr] && reg_map[V] != -1) {
            // spill that instruction
            return V;
        }
    }
    return NULL; // if no instructions match 
}

void registerAllocation(LLVMModuleRef module) {
    for (LLVMValueRef function = LLVMGetFirstFunction(module); function; function = LLVMGetNextFunction(function)) {
        for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(function); BB; BB = LLVMGetNextBasicBlock(BB)) {
            int available_registers[NUM_REGISTERS] = {1, 1, 1}; // ebx, ecx, edx
            std::map<LLVMValueRef, int> reg_map;
            std::vector<LLVMValueRef> sorted_list;

            // Call compute_liveness to populate inst_index and live_range
            auto [inst_index, live_range] = compute_liveness(BB);
            
            // Populate sorted_list with instructions in the basic block
            for (LLVMValueRef Instr = LLVMGetFirstInstruction(BB); Instr; Instr = LLVMGetNextInstruction(Instr)) {
                sorted_list.push_back(Instr);
            }
            
            // Sort sorted_list based on the end of the live range in descending order
            std::sort(sorted_list.begin(), sorted_list.end(), [&live_range](LLVMValueRef a, LLVMValueRef b) {
                return live_range[a].second > live_range[b].second;
            });

            // Register allocation process
            for (LLVMValueRef Instr = LLVMGetFirstInstruction(BB); Instr; Instr = LLVMGetNextInstruction(Instr)) {
                if (LLVMGetInstructionOpcode(Instr) == LLVMAlloca) {
                    continue; // Ignore alloc instructions
                }

                if (LLVMGetInstructionOpcode(Instr) == LLVMStore || LLVMGetInstructionOpcode(Instr) == LLVMBr || LLVMGetInstructionOpcode(Instr) == LLVMCall) {
                    for (unsigned i = 0; i < LLVMGetNumOperands(Instr); ++i) {
                        LLVMValueRef operand = LLVMGetOperand(Instr, i);
                        // If live range of any operand of Instr ends, and it has a physical register P assigned
                        // to it then add P to available set of registers.
                        if (live_range[operand].second == inst_index[Instr] && reg_map[operand] != -1) {
                            available_registers[reg_map[operand]] = 1;
                        }
                    }
                    continue;
                }

                if (LLVMGetInstructionOpcode(Instr) == LLVMAdd || LLVMGetInstructionOpcode(Instr) == LLVMMul || LLVMGetInstructionOpcode(Instr) == LLVMSub) {
                    LLVMValueRef first_operand = LLVMGetOperand(Instr, 0);
                    LLVMValueRef second_operand = LLVMGetOperand(Instr, 1);

                    if (reg_map[first_operand] != -1 && live_range[first_operand].second == inst_index[Instr]) {
                        reg_map[Instr] = reg_map[first_operand];
                    } else if (available_registers[0] || available_registers[1] || available_registers[2]) {  //if physical registers are available
                        for (int i = 0; i < NUM_REGISTERS; ++i) {
                            if (available_registers[i]) {
                                reg_map[Instr] = i;
                                available_registers[i] = 0;
                                break;
                            }
                        }
                    } else {   // if physical register is not available 
                        LLVMValueRef V = find_spill(Instr, reg_map, inst_index, sorted_list, live_range);
                        if (live_range[V].second > live_range[Instr].second) {
                            reg_map[Instr] = -1;
                        } else {
                            reg_map[Instr] = reg_map[V];
                            reg_map[V] = -1;
                        }
                    }

                    // If the live range of the first operand ends, add its register to the available set
                    if (live_range[first_operand].second == inst_index[Instr] && reg_map[first_operand] != -1) {
                        available_registers[reg_map[first_operand]] = 1;
                    }

                    // If the live range of the second operand ends, add its register to the available set
                    if (live_range[second_operand].second == inst_index[Instr] && reg_map[second_operand] != -1) {
                        available_registers[reg_map[second_operand]] = 1;
                    }
                }

                // If the live range of any operand ends, add its register to the available set
                for (unsigned i = 0; i < LLVMGetNumOperands(Instr); ++i) {
                    LLVMValueRef operand = LLVMGetOperand(Instr, i);
                    if (live_range[operand].second == inst_index[Instr] && reg_map[operand] != -1) {
                        available_registers[reg_map[operand]] = 1;
                    }
                }
            }
        }
    }
}


