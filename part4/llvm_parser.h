#ifndef LLVM_PARSER_H
#define LLVM_PARSER_H

#include <llvm-c/Core.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using instructionSet = std::unordered_set<LLVMValueRef>;
using bbMap = std::unordered_map<LLVMBasicBlockRef, instructionSet>;
using predMap = std::unordered_map<LLVMBasicBlockRef, std::vector<LLVMBasicBlockRef>>;

LLVMModuleRef createLLVMModel(char *filename);

// Function that builds a map of basic blocks to their predecessors
predMap buildPredMap(LLVMValueRef function);

// Struct to hold opcode and operands for instructions
struct InstructionKey {
    LLVMOpcode opcode;
    LLVMValueRef operand1;
    LLVMValueRef operand2;

    InstructionKey(LLVMOpcode op, LLVMValueRef op1, LLVMValueRef op2 = NULL)
        : opcode(op), operand1(op1), operand2(op2) {}

    bool operator==(const InstructionKey& other) const {
        return opcode == other.opcode && operand1 == other.operand1 && operand2 == other.operand2;
    }
};

// Hash function for InstructionKey
struct InstructionKeyHash {
    std::size_t operator()(const InstructionKey& key) const {
        size_t hash = std::hash<LLVMOpcode>{}(key.opcode);
        hash ^= std::hash<LLVMValueRef>{}(key.operand1) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        if (key.operand2) {
            hash ^= std::hash<LLVMValueRef>{}(key.operand2) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

// Function that performs CSE
bool commonSubExprx(LLVMBasicBlockRef bb);
// function that performs dead code elimination
bool deadCode(LLVMBasicBlockRef bb);

// Function that performs constant folding
bool constantFolding(LLVMBasicBlockRef bb);

// Helper function that calls local optimizations on a given basic block
bool performLocalOptimizations(LLVMBasicBlockRef bb);

// Helper function that applies local optimizations
bool applyLocalOptimizations(LLVMValueRef function);

// Helper function that creates the GEN Map 
bbMap getGenMap(LLVMValueRef function);

// Helper function that creates the KILL Map
bbMap getKillMap(LLVMValueRef Function);

// function that crates the in and out sets 
bbMap getInMap(LLVMValueRef Function, const bbMap &GenMap, bbMap &KillMap, const predMap &predecessorMap);

// Helper function that calls for global optimizations on the GEN and KILL set
bool applyGlobalOptimizations(LLVMValueRef function, const predMap &predecessorMap);

// Helper function that removes redundant load instructions based on the IN map
bool removeRedundantLoads(LLVMValueRef Function, bbMap &InMap);

// Function that loops through global and local optimizations
void doOptimizations(LLVMValueRef function);

void walkBasicblocks(LLVMValueRef function);
void walkFunctions(LLVMModuleRef module);
void walkGlobalValues(LLVMModuleRef module);

#endif // LLVM_PARSER_H
