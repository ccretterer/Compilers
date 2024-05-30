/*
*   Purpose:  This file generates the assembly code from our optimized LLVM code after part 3. It makes use of four helper functions: 
*   createBBLabels, printDirectives, printFunctionEnd, getOffsetMap. It uses algorithms as described on Canvas.
*   Author: Carly Retterer
*   Date: 30 May 2024
*/

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

void generateAssembly(LLVMModuleRef module) {
    // Iterate through each function in the module
    for (LLVMValueRef function = LLVMGetFirstFunction(module); function; function = LLVMGetNextFunction(function)) {
        // Initialize local variables
        std::map<LLVMBasicBlockRef, std::string> bb_labels;
        int localMem = 4;
        std::map<LLVMValueRef, int> offset_map;
        
        // Call helper functions
        createBBLabels(module, bb_labels);
        printDirectives(function);
        getOffsetMap(module, function, localMem, offset_map);
        
        // Emit function prologue
        emit("pushl %%ebp");
        emit("movl %%esp, %%ebp");
        emit("subl $%d, %%esp", localMem);
        emit("pushl %%ebx");
        
        // Iterate through each basic block
        for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(function); BB; BB = LLVMGetNextBasicBlock(BB)) {
            // Print the basic block label
            emit("%s:", bb_labels[BB].c_str());
            
            // Iterate through each instruction
            for (LLVMValueRef Instr = LLVMGetFirstInstruction(BB); Instr; Instr = LLVMGetNextInstruction(Instr)) {
                // Handle different types of instructions (return, load, store, call, branch, arithmetic, compare)
                switch (LLVMGetInstructionOpcode(Instr)) {
                    case LLVMRet: {
                        LLVMValueRef A = LLVMGetOperand(Instr, 0);
                        //const instruc
                        if (LLVMIsConstant(A)) {
                            emit("movl $%d, %%eax", LLVMConstIntGetZExtValue(A));
                        } else {
                            int offset = offset_map[A];
                            emit("movl %d(%%ebp), %%eax", offset);
                        }
                        emit("popl %%ebx");
                        printFunctionEnd();
                        break;
                    }
                    // load instruc
                    case LLVMLoad: {
                        LLVMValueRef b = LLVMGetOperand(Instr, 0);
                        if (b) {
                            int offset = offset_map[b];
                            emit("movl %d(%%ebp), %%eax", offset); // Replace %%eax with %exx if assigned
                        }
                        break;
                    }
                    // store instruc
                    case LLVMStore: {
                        LLVMValueRef A = LLVMGetOperand(Instr, 0);
                        LLVMValueRef b = LLVMGetOperand(Instr, 1);
                        int offset_b = offset_map[b];
                        if (LLVMIsConstant(A)) {
                            emit("movl $%d, %d(%%ebp)", LLVMConstIntGetZExtValue(A), offset_b);
                        } else {
                            int offset_a = offset_map[A];
                            emit("movl %d(%%ebp), %%eax", offset_a); // Assume %a has been assigned a physical register %exx
                            emit("movl %%eax, %d(%%ebp)", offset_b);
                        }
                        break;
                    }
                    // call instruc
                    case LLVMCall: {
                        LLVMValueRef func = LLVMGetCalledValue(Instr);
                        emit("pushl %%ecx");
                        emit("pushl %%edx");
                        unsigned numOperands = LLVMGetNumOperands(Instr);
                        for (unsigned i = 0; i < numOperands - 1; i++) {
                            LLVMValueRef param = LLVMGetOperand(Instr, i);
                            if (LLVMIsConstant(param)) {
                                emit("pushl $%d", LLVMConstIntGetZExtValue(param));
                            } else {
                                int offset = offset_map[param];
                                emit("pushl %d(%%ebp)", offset);
                            }
                        }
                        emit("call %s", LLVMGetValueName(func));
                        for (unsigned i = 0; i < numOperands - 1; i++) {
                            emit("addl $4, %%esp");
                        }
                        emit("popl %%edx");
                        emit("popl %%ecx");
                        break;
                    }
                    // branch instruc 
                    case LLVMBr: {
                        if (LLVMIsConditional(Instr)) {
                            LLVMValueRef cond = LLVMGetOperand(Instr, 0);
                            int offset_cond = offset_map[cond];
                            LLVMValueRef label_true = LLVMGetOperand(Instr, 1);
                            LLVMValueRef label_false = LLVMGetOperand(Instr, 2);
                            emit("cmpl $0, %d(%%ebp)", offset_cond);
                            emit("jne %s", bb_labels[LLVMValueAsBasicBlock(label_true)].c_str());
                            emit("jmp %s", bb_labels[LLVMValueAsBasicBlock(label_false)].c_str());
                        } else {
                            LLVMValueRef label = LLVMGetOperand(Instr, 0);
                            emit("jmp %s", bb_labels[LLVMValueAsBasicBlock(label)].c_str());
                        }
                        break;
                    }
                    // arithmetic instruc 
                    case LLVMAdd:
                    case LLVMMul:
                    case LLVMSub: {
                        LLVMValueRef a = LLVMGetOperand(Instr, 0);
                        LLVMValueRef b = LLVMGetOperand(Instr, 1);
                        int offset_a = offset_map[a];
                        int offset_b = offset_map[b];
                        emit("movl %d(%%ebp), %%eax", offset_a);
                        if (LLVMIsConstant(b)) {
                            emit("addl $%d, %%eax", LLVMConstIntGetZExtValue(b)); // Replace addl by subl or imull based on opcode
                        } else {
                            emit("addl %d(%%ebp), %%eax", offset_b); // Replace addl by subl or imull based on opcode
                        }
                        emit("movl %%eax, %d(%%ebp)", offset_map[Instr]);
                        break;
                    }
                    // comparing instruc 
                    case LLVMICmp: {
                        LLVMValueRef a = LLVMGetOperand(Instr, 0);
                        LLVMValueRef b = LLVMGetOperand(Instr, 1);
                        int offset_a = offset_map[a];
                        int offset_b = offset_map[b];
                        emit("movl %d(%%ebp), %%eax", offset_a);
                        if (LLVMIsConstant(b)) {
                            emit("cmpl $%d, %%eax", LLVMConstIntGetZExtValue(b));
                        } else {
                            emit("cmpl %d(%%ebp), %%eax", offset_b);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
}
// helper function to populates a map where the key is an 
// LLVMBasicBlockRef and the associated value is a char *, which you can use as a label when generating code.
void createBBLabels(LLVMModuleRef module, std::map<LLVMBasicBlockRef, std::string>& bb_labels) {
    // Populate bb_labels map
    int index = 0;
    for (LLVMValueRef function = LLVMGetFirstFunction(module); function; function = LLVMGetNextFunction(function)) {
        for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(function); BB; BB = LLVMGetNextBasicBlock(BB)) {
            bb_labels[BB] = "BB" + std::to_string(index++);
        }
    }
}

// helper function to emit the required directives for your function.
void printDirectives(LLVMValueRef function) {
    emit(".text");
    emit(".globl %s", LLVMGetValueName(function));
    emit(".type %s, @function", LLVMGetValueName(function));
    emit("%s:", LLVMGetValueName(function));
}

// helper function to emits the assembly instructions to restore the value of 
// %esp and %ebp (you can do this by using the leave instruction instead of explicit moves), and the ret instruction. 
void printFunctionEnd() {
    emit("leave");
    emit("ret");
}


// helper function to populate a map offset_map. This map 
// associates each value(instruction) to the memory offset of that value from %ebp. 
// The keys in this map are LLVMValueRef and values are integers. This function 
// will also initialize an integer variable localMem that indicates the number of bytes required to store the local values. 
void getOffsetMap(LLVMModuleRef module, LLVMValueRef function, int& localMem, std::map<LLVMValueRef, int>& offset_map) {
    localMem = 4;

    // If the function has a parameter
    if (LLVMCountParams(function) > 0) {
        LLVMValueRef param = LLVMGetParam(function, 0);
        offset_map[param] = 8;
    }

    for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(function); BB; BB = LLVMGetNextBasicBlock(BB)) {
        for (LLVMValueRef instr = LLVMGetFirstInstruction(BB); instr; instr = LLVMGetNextInstruction(instr)) {
            // If instr is an alloc instruction
            if (LLVMGetInstructionOpcode(instr) == LLVMAlloca) {
                localMem += 4;
                offset_map[instr] = -localMem;
            }
            // If instr is a store instruction
            else if (LLVMGetInstructionOpcode(instr) == LLVMStore) {
                LLVMValueRef first_operand = LLVMGetOperand(instr, 0);
                LLVMValueRef second_operand = LLVMGetOperand(instr, 1);
                
                if (first_operand == LLVMGetParam(function, 0)) {
                    int x = offset_map[first_operand];
                    offset_map[second_operand] = x;
                } else {
                    int x = offset_map[second_operand];
                    offset_map[first_operand] = x;
                }
            }
            // If instr is a load instruction
            else if (LLVMGetInstructionOpcode(instr) == LLVMLoad) {
                LLVMValueRef first_operand = LLVMGetOperand(instr, 0);
                int x = offset_map[first_operand];
                offset_map[instr] = x;
            }
        }
    }
}

// utility function to format and print strings 
void emit(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}
