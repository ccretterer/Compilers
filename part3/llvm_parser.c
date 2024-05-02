/*
* Purpose: This file takes in LLVM IR and optimizes it using commmon subexpression elimination, dead code elimination,
* and constant folding. It then uses constant propagation. 
* Author: Carly Retterer
* Date: 2 May 2024
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>
#include <unordered_set>
#include <string>
#include <ostream>
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstddef>
#include "llvm_parser.h"


#define prt(x) if(x) { printf("%s\n", x); }

// Function to read the given LLVM file and load the LLVM IR into data structures for optimization
LLVMModuleRef createLLVMModel(char *filename){
    char *err = NULL;
    LLVMMemoryBufferRef ll_f = NULL;
    LLVMModuleRef m = NULL;
    LLVMCreateMemoryBufferWithContentsOfFile(filename, &ll_f, &err);
    if (err != NULL) {
        prt(err);
        return NULL;
    }
    LLVMParseIRInContext(LLVMGetGlobalContext(), ll_f, &m, &err);
    if (err != NULL) {
        prt(err);
    }
    return m;
}

// Custom data structures for optimization
using instructionSet = std::unordered_set<LLVMValueRef>;
using bbMap = std::unordered_map<LLVMBasicBlockRef, instructionSet>;
using predMap = std::unordered_map<LLVMBasicBlockRef, std::vector<LLVMBasicBlockRef>>;

// Function to build a map of basic blocks to their predecessors
predMap buildPredMap(LLVMValueRef function) {
    predMap predecessors;

    // Initialize the map with empty vectors for all basic blocks
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb != NULL; bb = LLVMGetNextBasicBlock(bb)) {
        predecessors[bb] = std::vector<LLVMBasicBlockRef>();
    }

    // Populate the predecessor map
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb != NULL; bb = LLVMGetNextBasicBlock(bb)) {
        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(bb);
        if (terminator == NULL) {
            continue;
        }
        int numSuccessors = LLVMGetNumSuccessors(terminator);
        for (int i = 0; i < numSuccessors; ++i) {
            LLVMBasicBlockRef successor = LLVMGetSuccessor(terminator, i);
            if (successor != NULL) {
                predecessors[successor].push_back(bb);
            }
        }
    }
    return predecessors;
}

// Function to perform Common Subexpression Elimination (commonSubExprx) on a basic block
bool commonSubExprx(LLVMBasicBlockRef basicBlock) {
    if (basicBlock == NULL) {
        printf("Has to skip a basic block in commonSubExprx.\n");
        return false;
    }

    std::unordered_map<InstructionKey, LLVMValueRef, InstructionKeyHash> cachedExpressions;
    bool hasChanges = false;

    printf("Entering commonSubExprx for basic block: %p\n", basicBlock);

    LLVMValueRef currentInstr = LLVMGetFirstInstruction(basicBlock);
    while (currentInstr != NULL) {
        LLVMValueRef nextInstr = LLVMGetNextInstruction(currentInstr); // Fetch next instruction before potentially deleting the current one

        LLVMOpcode opcode = LLVMGetInstructionOpcode(currentInstr);
        int operandCount = LLVMGetNumOperands(currentInstr);
        LLVMValueRef firstOperand = (operandCount > 0) ? LLVMGetOperand(currentInstr, 0) : NULL;
        LLVMValueRef secondOperand = (operandCount > 1) ? LLVMGetOperand(currentInstr, 1) : NULL;

        if (!firstOperand) {
            currentInstr = nextInstr;
            continue;
        }

        InstructionKey instrKey(opcode, firstOperand, secondOperand);
        auto exprEntry = cachedExpressions.find(instrKey);

        printf("Processing instruction: %p, opcode: %d, operand0: %p, operand1: %p\n", currentInstr, opcode, firstOperand, secondOperand);

        // Check if an existing instruction was found
        if (exprEntry != cachedExpressions.end()) {
            printf("Found existing instruction: %p\n", exprEntry->second);
            LLVMValueRef foundInstr = exprEntry->second;

            if (opcode == LLVMLoad && LLVMGetInstructionOpcode(foundInstr) == LLVMLoad) {
                // Check for intervening stores that might affect load safety
                bool isCSESafe = true;
                for (LLVMValueRef checkInstr = LLVMGetNextInstruction(foundInstr);
                     checkInstr != currentInstr;
                     checkInstr = LLVMGetNextInstruction(checkInstr)) {
                    if (checkInstr == NULL) {
                        isCSESafe = false;
                        break;
                    }
                    if (LLVMGetInstructionOpcode(checkInstr) == LLVMStore) {
                        LLVMValueRef storeAddress = LLVMGetOperand(checkInstr, 1);
                        if (storeAddress == firstOperand) {
                            isCSESafe = false;
                            printf("Unsafe to perform CSE between %p and %p due to intervening store %p\n", foundInstr, currentInstr, checkInstr);
                            break;
                        }
                    }
                }

                if (isCSESafe) {
                    LLVMReplaceAllUsesWith(currentInstr, foundInstr);
                    LLVMInstructionEraseFromParent(currentInstr);
                    hasChanges = true;
                    printf("Performed CSE: Replaced %p with %p\n", currentInstr, foundInstr);
                }
            } else {
                LLVMReplaceAllUsesWith(currentInstr, foundInstr);
                LLVMInstructionEraseFromParent(currentInstr);
                hasChanges = true;
                printf("Performed CSE: Replaced %p with %p for non-load instruction\n", currentInstr, foundInstr);
            }
        } else {
            cachedExpressions[instrKey] = currentInstr;
            printf("Storing new instruction key for %p\n", currentInstr);
        }
        currentInstr = nextInstr; // Move to the next instruction
    }

    printf("Exiting CSE for basic block: %p, changed: %d\n", basicBlock, hasChanges);
    return hasChanges;
}



// Function to perform Dead Code Elimination (DCE) on a basic block
bool deadCode(LLVMBasicBlockRef basicBlock) {
    if (basicBlock == NULL) {
        return false;
    }

    bool isModified = false;

    printf("Entering DCE for basic block: %p\n", (void*)basicBlock);

    // Set of instructions identified as unnecessary and safe to remove
    instructionSet candidatesForRemoval;

    for (LLVMValueRef currentInstr = LLVMGetFirstInstruction(basicBlock); currentInstr != NULL;
         currentInstr = LLVMGetNextInstruction(currentInstr)) {
        LLVMOpcode instructionCode = LLVMGetInstructionOpcode(currentInstr);
        printf("Processing instruction: %p, opcode: %d\n", (void*)currentInstr, instructionCode);

        bool isUsed = LLVMGetFirstUse(currentInstr) != NULL;

        // Determine if the instruction has effects beyond its immediate value
        bool retainsEffects = false;
        switch (instructionCode) {
            case LLVMStore:
            case LLVMCall:
            case LLVMRet:
            case LLVMBr:
            case LLVMFence:
            case LLVMAtomicCmpXchg:
            case LLVMAtomicRMW:
                retainsEffects = true;
                break;
            default:
                retainsEffects = false;
        }

        // Conditionally mark unused and effect-free instructions for removal
        if (!isUsed && !retainsEffects) {
            printf("Marking instruction for removal: %p\n", (void*)currentInstr);
            candidatesForRemoval.insert(currentInstr);
        } else {
            printf("Keeping instruction: %p\n", (void*)currentInstr);
        }
    }

    // Execute the removal of all marked instructions
    for (LLVMValueRef deadInstr : candidatesForRemoval) {
        printf("Removing instruction: %p\n", (void*)deadInstr);
        LLVMInstructionEraseFromParent(deadInstr);
        isModified = true;
    }

    printf("Exiting DCE for basic block: %p, changed: %d\n", (void*)basicBlock, isModified);

    return isModified;
}

// Function to perform Constant Folding (constantFolding) on a basic block
bool constantFolding(LLVMBasicBlockRef basicBlock) {
    if (basicBlock == NULL) {
        return false;
    }

    bool isModified = false;

    printf("Entering CF for basic block: %p\n", (void*)basicBlock);

    for (LLVMValueRef currentInstr = LLVMGetFirstInstruction(basicBlock); currentInstr != NULL;
         currentInstr = LLVMGetNextInstruction(currentInstr)) {
        LLVMOpcode opcode = LLVMGetInstructionOpcode(currentInstr);
        int operandCount = LLVMGetNumOperands(currentInstr);
        printf("Processing instruction: %p, opcode: %d, num_ops: %d\n", (void*)currentInstr, opcode, operandCount);

        if (operandCount > 1) {
            LLVMValueRef firstOperand = LLVMGetOperand(currentInstr, 0);
            LLVMValueRef secondOperand = LLVMGetOperand(currentInstr, 1);

            // Check if both operands are constants
            if (firstOperand != NULL && secondOperand != NULL && LLVMIsConstant(firstOperand) && LLVMIsConstant(secondOperand)) {
                printf("Both operands are constants\n");
                LLVMValueRef foldedConst = NULL;

                // Perform constant folding based on the operation
                switch (opcode) {
                    case LLVMAdd:
                        foldedConst = LLVMConstAdd(firstOperand, secondOperand);
                        break;
                    case LLVMSub:
                        foldedConst = LLVMConstSub(firstOperand, secondOperand);
                        break;
                    case LLVMMul:
                        foldedConst = LLVMConstMul(firstOperand, secondOperand);
                        break;
                    default:
                        printf("Unsupported opcode for constant folding: %d\n", opcode);
                        break;
                }

                // Replace the original instruction with the new constant if folding was successful
                if (foldedConst != NULL) {
                    printf("Replacing instruction: %p with constant: %p\n", (void*)currentInstr, (void*)foldedConst);
                    LLVMReplaceAllUsesWith(currentInstr, foldedConst);
                    //LLVMInstructionEraseFromParent(currentInstr);
                    isModified = true;
                }
            } else {
                printf("At least one operand is not a constant\n");
            }
        }
    }

    printf("Exiting CF for basic block: %p, changed: %d\n", (void*)basicBlock, isModified);

    return isModified;
}

// Function to create a map of basic blocks to their GEN sets
bbMap getGenMap(LLVMValueRef targetFunction) {
    bbMap genMap;
    printf("Starting get GenMap\n");

    // Iterate over each basic block in the function
    for (LLVMBasicBlockRef currentBlock = LLVMGetFirstBasicBlock(targetFunction);
         currentBlock != NULL;
         currentBlock = LLVMGetNextBasicBlock(currentBlock)) {

        printf("Processing Basic Block: %p\n", (void*)currentBlock);
        instructionSet uniqueStores;

        // Iterate over each instruction in the basic block
        for (LLVMValueRef currentInstr = LLVMGetFirstInstruction(currentBlock);
             currentInstr != NULL;
             currentInstr = LLVMGetNextInstruction(currentInstr)) {

            printf("Examining Instruction: %p\n", (void*)currentInstr);

            // If this is a store instruction
            if (LLVMGetInstructionOpcode(currentInstr) == LLVMStore) {
                LLVMValueRef storeLoc = LLVMGetOperand(currentInstr, 1);
                printf("Store Instruction at Address: %p\n", (void*)storeLoc);

                // Remove any previous store instructions to the same address
                auto iter = uniqueStores.begin();
                while (iter != uniqueStores.end()) {
                    if (LLVMGetOperand(*iter, 1) == storeLoc) {
                        printf("Removing overlapping store instruction: %p\n", (void*)*iter);
                        iter = uniqueStores.erase(iter);
                    } else {
                        ++iter;
                    }
                }

                // Add the current store instruction to the set
                uniqueStores.insert(currentInstr);
                printf("Inserted Store Instruction: %p\n", (void*)currentInstr);
            }
        }

        // Store the set in the GEN map
        genMap[currentBlock] = std::move(uniqueStores);
    }

    printf("Finished createGenMap\n");
    return genMap;
}


bbMap getKillMap(LLVMValueRef Function) {
    bbMap killMap;
    instructionSet allStores;

    printf("Collecting all Store Instructions\n");

    LLVMBasicBlockRef CurrentBlock = LLVMGetEntryBasicBlock(Function);
    while (CurrentBlock != NULL) {
        printf("Accessing Basic Block %p\n", (void*)CurrentBlock);
        LLVMValueRef Instruction = LLVMGetFirstInstruction(CurrentBlock);
        printf("First Instruction in Basic Block %p is %p\n", (void*)CurrentBlock, (void*)Instruction);
        while (Instruction != NULL) {
            printf("Processing Instruction %p, Opcode: %d\n", (void*)Instruction, LLVMGetInstructionOpcode(Instruction));
            if (LLVMGetInstructionOpcode(Instruction) == LLVMStore) {
                if (LLVMGetOperand(Instruction, 1) == NULL) {
                    printf("Failed to get store address for Instruction %p\n", (void*)Instruction);
                    Instruction = LLVMGetNextInstruction(Instruction);
                    continue;
                }
                allStores.insert(Instruction);
                printf("Added Store Instruction to allStores: %p\n", (void*)Instruction);
            }
            Instruction = LLVMGetNextInstruction(Instruction);
        }
        CurrentBlock = LLVMGetNextBasicBlock(CurrentBlock);
    }

    printf("Computing Kill Sets for each Basic Block\n");
    CurrentBlock = LLVMGetEntryBasicBlock(Function);
    while (CurrentBlock != NULL) {
        instructionSet killSet;

        LLVMValueRef Instruction = LLVMGetFirstInstruction(CurrentBlock);
        while (Instruction != NULL) {
            if (LLVMGetInstructionOpcode(Instruction) == LLVMStore) {
                LLVMValueRef address = LLVMGetOperand(Instruction, 1);
                printf("Processing Store Instruction at %p for Kill Set\n", (void*)address);

                for (LLVMValueRef otherStoreInst : allStores) {
                    if (LLVMGetOperand(otherStoreInst, 1) != NULL && LLVMGetOperand(otherStoreInst, 1) == address && otherStoreInst != Instruction) {
                        killSet.insert(otherStoreInst);
                        printf("Adding to Kill Set, Store Instruction: %p\n", (void*)otherStoreInst);
                    }
                }
            }
            Instruction = LLVMGetNextInstruction(Instruction);
        }

        killMap[CurrentBlock] = std::move(killSet);
        printf("Kill Set for Basic Block %p populated\n", (void*)CurrentBlock);
        CurrentBlock = LLVMGetNextBasicBlock(CurrentBlock);
    }

    printf("Returning from getting Kill Map\n");
    return killMap;
}

// Computing in and out
// Only need to return the in as out is only used in this function
// Compute the 'in' set
// Function to calculate IN maps for basic blocks based on GEN and KILL maps
bbMap getInMap(LLVMValueRef targetFunction, const bbMap &genSets, bbMap &killSets, const predMap &predecessorsMap) {
    bbMap inMap;
    bbMap outMap;

    printf("Initializing IN sets for each basic block to empty.\n");
    LLVMBasicBlockRef currBlock = LLVMGetEntryBasicBlock(targetFunction);
    while (currBlock != NULL) {
        inMap[currBlock] = instructionSet();
        printf("IN set initialized for block %p\n", (void*)currBlock);
        currBlock = LLVMGetNextBasicBlock(currBlock);
    }

    printf("Initializing OUT sets for each basic block using GEN sets.\n");
    currBlock = LLVMGetEntryBasicBlock(targetFunction);
    while (currBlock != NULL) {
        outMap[currBlock] = genSets.at(currBlock);
        printf("OUT set for block %p initialized from GEN set.\n", (void*)currBlock);
        currBlock = LLVMGetNextBasicBlock(currBlock);
    }

    bool changesDetected = true;
    printf("Starting computation of IN and OUT sets.\n");
    while (changesDetected) {
        changesDetected = false;
        currBlock = LLVMGetEntryBasicBlock(targetFunction);
        while (currBlock != NULL) {
            printf("Computing IN and OUT for block %p\n", (void*)currBlock);

            instructionSet newInSet;
            const std::vector<LLVMBasicBlockRef>& blockPredecessors = predecessorsMap.at(currBlock);
            printf("Block %p has %zu predecessors.\n", (void*)currBlock, blockPredecessors.size());
            for (LLVMBasicBlockRef predecessor : blockPredecessors) {
                newInSet.insert(outMap[predecessor].begin(), outMap[predecessor].end());
                printf("Merging OUT of predecessor %p into IN of %p.\n", (void*)predecessor, (void*)currBlock);
            }

            instructionSet oldOut = outMap[currBlock];
            outMap[currBlock] = genSets.at(currBlock);
            const instructionSet &currentKillSet = killSets.at(currBlock);
            for (LLVMValueRef inst : newInSet) {
                if (currentKillSet.find(inst) == currentKillSet.end()) {
                    outMap[currBlock].insert(inst);
                }
            }

            printf("Updated OUT set for block %p.\n", (void*)currBlock);
            if (outMap[currBlock] != oldOut) {
                changesDetected = true;
                printf("OUT set for block %p has changed.\n", (void*)currBlock);
            }

            inMap[currBlock] = std::move(newInSet);
            currBlock = LLVMGetNextBasicBlock(currBlock);
        }
    }

    printf("Finished computing IN and OUT sets.\n");
    return inMap;
}

// Function to remove redundant load instructions based on an IN map
bool removeRedundantLoads(LLVMValueRef targetFunction, bbMap &inSets) {
    if (targetFunction == NULL) {
        // Skip null functions
        return false;
    }

    bool isModified = false;
    LLVMBasicBlockRef currentBlock = LLVMGetEntryBasicBlock(targetFunction);

    printf("Starting removal of redundant load instructions.\n");

    while (currentBlock != NULL) {
        instructionSet activeInstructions = inSets.at(currentBlock);
        LLVMValueRef currentInstruction = LLVMGetFirstInstruction(currentBlock);

        // Store instructions to be deleted after the loop
        instructionSet instructionsToDelete;

        printf("Processing block %p\n", (void*)currentBlock);

        while (currentInstruction != NULL) {
            LLVMValueRef nextInstruction = LLVMGetNextInstruction(currentInstruction); // Safe pointer for iteration

            if (LLVMGetInstructionOpcode(currentInstruction) == LLVMStore) {
                LLVMValueRef storeAddress = LLVMGetOperand(currentInstruction, 1);
                printf("Processing store instruction %p at address %p\n", (void*)currentInstruction, (void*)storeAddress);

                // Remove any overlapping store instructions to the same address
                auto iter = activeInstructions.begin();
                while (iter != activeInstructions.end()) {
                    if (LLVMGetOperand(*iter, 1) == storeAddress) {
                        printf("Removing redundant store instruction %p from active set\n", (void*)*iter);
                        iter = activeInstructions.erase(iter);
                    } else {
                        ++iter;
                    }
                }
                // Add the current store instruction to the set
                activeInstructions.insert(currentInstruction);
            } else if (LLVMGetInstructionOpcode(currentInstruction) == LLVMLoad) {
                LLVMValueRef loadAddress = LLVMGetOperand(currentInstruction, 1);
                printf("Processing load instruction %p at address %p\n", (void*)currentInstruction, (void*)loadAddress);

                // Collect all store instructions that write to the same address
                std::vector<LLVMValueRef> matchingStores;
                for (LLVMValueRef storeInst : activeInstructions) {
                    if (LLVMGetOperand(storeInst, 1) == loadAddress) {
                        matchingStores.push_back(storeInst);
                    }
                }
                // Verify if all these stores are constant and have the same value
                bool constantStores = true;
                LLVMValueRef constantValue = NULL;
                for (LLVMValueRef storeInst : matchingStores) {
                    if (!LLVMIsConstant(LLVMGetOperand(storeInst, 0))) {
                        constantStores = false;
                        break;
                    }
                    if (constantValue == NULL) {
                        constantValue = LLVMGetOperand(storeInst, 0);
                    } else if (constantValue != LLVMGetOperand(storeInst, 0)) {
                        constantStores = false;
                        break;
                    }
                }
                if (constantStores && constantValue) {
                    printf("Replacing load instruction %p with constant value from store %p\n", (void*)currentInstruction, (void*)constantValue);
                    LLVMReplaceAllUsesWith(currentInstruction, constantValue);
                    instructionsToDelete.insert(currentInstruction);
                }
            }
            currentInstruction = nextInstruction;
        }
        // Remove all marked instructions
        for (LLVMValueRef inst : instructionsToDelete) {
            printf("Deleting load instruction %p\n", (void*)inst);
            LLVMInstructionEraseFromParent(inst);
            isModified = true;
        }
        // Update the InMap for the current basic block with only the active instructions
        instructionSet updatedActiveInstructions;
        for (LLVMValueRef inst = LLVMGetFirstInstruction(currentBlock); inst != NULL; inst = LLVMGetNextInstruction(inst)) {
            if (instructionsToDelete.find(inst) == instructionsToDelete.end()) {
                updatedActiveInstructions.insert(inst);
            }
        }
        inSets[currentBlock] = std::move(updatedActiveInstructions);
        currentBlock = LLVMGetNextBasicBlock(currentBlock);
    }

    printf("Completed removal of redundant load instructions.\n");
    return isModified;
}


// Function to handle local optimizations on a single basic block
bool performLocalOptimizations(LLVMBasicBlockRef bb) {
    // CSE
    if (commonSubExprx(bb)) {
        return true;
    }
    // Dead Code
    if (deadCode(bb)) {
        return true;
    }
    // Constant Folding
    if (constantFolding(bb)) {
        return true;
    }
    // Return false in the case no optimizations are made
    return false;
}

// Walking through each basic block and applying local optimizations 
bool applyLocalOptimizations(LLVMValueRef function) {
    bool localChanges = false;
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
        if (performLocalOptimizations(bb)) {
            localChanges = true;
        }
    }
    return localChanges;
}

// Global optimizations
bool applyGlobalOptimizations(LLVMValueRef function, const predMap& predecessorMap) {
    bbMap genMap = getGenMap(function);
    bbMap killMap = getKillMap(function);
    bbMap inMap = getInMap(function, genMap, killMap, predecessorMap);
    return removeRedundantLoads(function, inMap);
}

// Main function orchestrating the optimization process
void doOptimizations(LLVMValueRef function) {
    if (LLVMIsDeclaration(function) || LLVMCountBasicBlocks(function) == 0) {
        return; // early exit for declarations or functions without basic blocks
    }

    predMap predecessorMap = buildPredMap(function);
    bool globalChanged, localChanged;  //bools to keep track if changes are made during local or global optimizations

    do {
        do {
            localChanged = applyLocalOptimizations(function);
        } while (localChanged);
        globalChanged = applyGlobalOptimizations(function, predecessorMap);
    } while (globalChanged);
}

void walkBasicblocks(LLVMValueRef function){

    for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
         basicBlock;
         basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
        
        printf("In basic block\n");
    
    }
}

void walkFunctions(LLVMModuleRef module) {
    for (LLVMValueRef function = LLVMGetFirstFunction(module);
         function;
         function = LLVMGetNextFunction(function)) {

        const char* funcName = LLVMGetValueName(function);

        printf("Function Name: %s\n", funcName);

        if (std::string(funcName) == "llvm.dbg.declare") {
            // Skip optimization for llvm.dbg.declare
            continue;
        }

        doOptimizations(function);
    }
}

void walkGlobalValues(LLVMModuleRef module){
    for (LLVMValueRef gVal =  LLVMGetFirstGlobal(module);
                                gVal;
                                gVal = LLVMGetNextGlobal(gVal)) {

            const char* gName = LLVMGetValueName(gVal);
            printf("Global variable name: %s\n", gName);
        }
}

// int main(int argc, char** argv)
// {
//     try {
//         LLVMModuleRef m = NULL;

//         if (argc == 2){
//             m = createLLVMModel(argv[1]);
//         }
//         else{
//             std::cerr << "Usage: " << argv[0] << " <LLVM IR file>" << std::endl;
//             return 1;
//         }

//         if (m != NULL){
//             LLVMDumpModule(m);

//             walkFunctions(m);

//             std::cout << "After modifications:" << std::endl;
//             LLVMDumpModule(m);

//             std::cout << "Optimization complete. Writing output to test_new.ll" << std::endl;
//             if (LLVMPrintModuleToFile(m, "test_new.ll", NULL) != 0) {
//                 std::cerr << "Failed to write output to test_new.ll" << std::endl;
//                 LLVMDisposeModule(m);
//                 return 1;
//             }

//             LLVMDisposeModule(m);
//             LLVMShutdown();
//         }
//         else {
//             std::cerr << "Failed to create LLVM module." << std::endl;
//             return 1;
//         }
//     } catch (const std::exception& e) {
//         std::cerr << "Error in main: " << e.what() << std::endl;
//         return 1;
//     }
    
//     return 0;
// }
