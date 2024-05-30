/*
*   Purpose: This is the h file for the llvm_builder.
*   Author: Carly Retterer
*   Date: 30 May 2024
*
*/

#ifndef LLVM_BUILDER_H
#define LLVM_BUILDER_H

#include <llvm-c/Core.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <map>
#include <string>
#include "ast.h"
#include "preprocessor.h"

// Global maps and variables
extern std::map<std::string, LLVMValueRef> var_map;
extern LLVMValueRef ret_ref;
extern LLVMBasicBlockRef retBB;

// Function Prototypes
LLVMBasicBlockRef genIRStmt(LLVMModuleRef mod, astNode* stmt, LLVMBuilderRef builder, LLVMBasicBlockRef startBB);
LLVMValueRef genIRExpr(LLVMModuleRef mod, astNode* expr, LLVMBuilderRef builder);
LLVMValueRef createBinaryOp(LLVMBuilderRef builder, op_type op, LLVMValueRef lhs, LLVMValueRef rhs);

void rename_variables(astNode* node);

LLVMValueRef functionTraversal(LLVMModuleRef mod, astNode* funcNode);

#endif // LLVM_BUILDER_H
