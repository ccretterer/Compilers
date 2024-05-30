/*
*   Purpose: This the llvm builder file that takes the AST and converts it to LLVM code. 
*   Author: Carly Retterer
*   Date: 30 May 2024
*/

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
std::map<std::string, LLVMValueRef> var_map;
LLVMValueRef ret_ref;
LLVMBasicBlockRef retBB;

// Function Prototypes
LLVMBasicBlockRef genIRStmt(LLVMModuleRef mod, astNode* stmt, LLVMBuilderRef builder, LLVMBasicBlockRef startBB);
LLVMValueRef genIRExpr(LLVMModuleRef mod, astNode* expr, LLVMBuilderRef builder);
LLVMValueRef createBinaryOp(LLVMBuilderRef builder, op_type op, LLVMValueRef lhs, LLVMValueRef rhs);

void rename_variables(astNode* node);

LLVMValueRef functionTraversal(LLVMModuleRef mod, astNode* funcNode) {
    printf("Starting functionTraversal\n");

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMTypeRef int32Type = LLVMInt32Type();
    LLVMTypeRef funcType = LLVMFunctionType(int32Type, NULL, 0, 0);
    LLVMValueRef func = LLVMAddFunction(mod, funcNode->func.name, funcType);
    LLVMBasicBlockRef entryBB = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(builder, entryBB);

    // Initialize var_map for parameters and local variables
    if (funcNode->func.param) {
        char* param_name = funcNode->func.param->var.name;
        LLVMValueRef param = LLVMGetParam(func, 0);
        LLVMValueRef alloc = LLVMBuildAlloca(builder, int32Type, param_name);
        LLVMBuildStore(builder, param, alloc);
        var_map[param_name] = alloc;
    }

    // Initialize ret_ref and retBB
    ret_ref = LLVMBuildAlloca(builder, int32Type, "ret_val");
    retBB = LLVMAppendBasicBlock(func, "return");

    // Generate IR for the function body
    if (funcNode->func.body) {
        printf("Generating IR for function body\n");
        LLVMBasicBlockRef exitBB = genIRStmt(mod, funcNode->func.body, builder, entryBB);
        // If the last block has no terminator, add a branch to retBB
        if (!LLVMGetBasicBlockTerminator(exitBB)) {
            LLVMPositionBuilderAtEnd(builder, exitBB);
            LLVMBuildBr(builder, retBB);
        }
    }

    // Generate return block
    LLVMPositionBuilderAtEnd(builder, retBB);
    LLVMValueRef ret_val = LLVMBuildLoad2(builder, int32Type, ret_ref, "");
    LLVMBuildRet(builder, ret_val);

    // Clean up
    LLVMDisposeBuilder(builder);
    var_map.clear();
    printf("Completed functionTraversal\n");
    return func;
}

// This is the basic block where the subroutine starts adding LLVM IR instructions
LLVMBasicBlockRef genIRStmt(LLVMModuleRef mod, astNode* stmt, LLVMBuilderRef builder, LLVMBasicBlockRef startBB) {
    printf("Generating IR for statement\n");
    LLVMPositionBuilderAtEnd(builder, startBB);

    switch (stmt->stmt.type) {
        //assignment statements 
        case ast_asgn: {
            printf("Generating IR for assignment\n");
            LLVMPositionBuilderAtEnd(builder, startBB); // Set the position of the builder
            LLVMValueRef rhs = genIRExpr(mod, stmt->stmt.asgn.rhs, builder); // Generate LLVMValueRef of RHS
            LLVMValueRef lhs = var_map[stmt->stmt.asgn.lhs->var.name]; // Retrieve LHS memory location
            LLVMBuildStore(builder, rhs, lhs); // Generate store instruction
            return startBB; // Return startBB as endBB
        }
        // call nodes
        case ast_call: {
            printf("Generating IR for function call\n");
            LLVMValueRef value = stmt->stmt.call.param ? genIRExpr(mod, stmt->stmt.call.param, builder) : NULL;
            if (strcmp(stmt->stmt.call.name, "print") == 0) {
                // Generate LLVMValueRef of the value being printed
                LLVMValueRef printValue = genIRExpr(mod, stmt->stmt.call.param, builder);
                // Generate a Call instruction to the print function with the value as a parameter
                LLVMBuildCall(builder, LLVMGetNamedFunction(mod, "print"), &printValue, 1, "");
            } else {
                LLVMBuildCall(builder, LLVMGetNamedFunction(mod, stmt->stmt.call.name), value ? &value : NULL, value ? 1 : 0, "");
            }
            return startBB;
        }

        // while nodes
        case ast_while: {
            printf("Generating IR for while loop\n");

            // Set the position of the builder to the end of startBB
            LLVMPositionBuilderAtEnd(builder, startBB);
            LLVMBasicBlockRef condBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "cond");
            LLVMBuildBr(builder, condBB);
            LLVMPositionBuilderAtEnd(builder, condBB);

            LLVMValueRef cond = genIRExpr(mod, stmt->stmt.whilen.cond, builder);
            LLVMBasicBlockRef trueBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "true");
            LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "false");
            LLVMBuildCondBr(builder, cond, trueBB, falseBB);

            // Generate the LLVM IR for the while loop body
            LLVMPositionBuilderAtEnd(builder, trueBB);
            LLVMBasicBlockRef trueExitBB = genIRStmt(mod, stmt->stmt.whilen.body, builder, trueBB);
            // Set the position of the builder to the end of trueExitBB
            LLVMPositionBuilderAtEnd(builder, trueExitBB);
            // Generate an unconditional branch to condBB at the end of trueExitBB
            LLVMBuildBr(builder, condBB);
            LLVMPositionBuilderAtEnd(builder, trueBB);
            return falseBB;
        }

        // if nodes 
        case ast_if: {
            printf("Generating IR for if statement\n");
            LLVMPositionBuilderAtEnd(builder, startBB);
            LLVMValueRef cond = genIRExpr(mod, stmt->stmt.ifn.cond, builder);

            // Generate two basic blocks, trueBB and falseBB
            LLVMBasicBlockRef trueBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "true");
            LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "false");
            LLVMBuildCondBr(builder, cond, trueBB, falseBB);

            // Handle the case where there is no else part
            if (!stmt->stmt.ifn.else_body) {
                LLVMPositionBuilderAtEnd(builder, trueBB);
                LLVMBasicBlockRef ifExitBB = genIRStmt(mod, stmt->stmt.ifn.if_body, builder, trueBB);
                LLVMPositionBuilderAtEnd(builder, ifExitBB);
                LLVMBuildBr(builder, falseBB);
                LLVMPositionBuilderAtEnd(builder, falseBB);
                return falseBB;
            }

            // Handle the case where there is an else part
            else {
                LLVMPositionBuilderAtEnd(builder, trueBB);
                LLVMBasicBlockRef ifExitBB = genIRStmt(mod, stmt->stmt.ifn.if_body, builder, trueBB);
                LLVMPositionBuilderAtEnd(builder, falseBB);
                LLVMBasicBlockRef elseExitBB = genIRStmt(mod, stmt->stmt.ifn.else_body, builder, falseBB);
                LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "end");
                LLVMPositionBuilderAtEnd(builder, ifExitBB);
                // Add an unconditional branch to endBB
                LLVMBuildBr(builder, endBB);
                
                // Set the position of the builder to the end of elseExitBB
                LLVMPositionBuilderAtEnd(builder, elseExitBB);
                LLVMBuildBr(builder, endBB);
                
                LLVMPositionBuilderAtEnd(builder, endBB);
                return endBB;
            }
        }

        case ast_ret: {
            printf("Generating IR for return statement\n");
            LLVMPositionBuilderAtEnd(builder, startBB);
            LLVMValueRef ret_val = genIRExpr(mod, stmt->stmt.ret.expr, builder);
            LLVMBuildStore(builder, ret_val, ret_ref);
            LLVMBuildBr(builder, retBB);
            LLVMBasicBlockRef afterRetBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "after_ret");
            LLVMPositionBuilderAtEnd(builder, afterRetBB);
            return afterRetBB;
        }
        case ast_block: {
            printf("Generating IR for block statement\n");
            LLVMBasicBlockRef prevBB = startBB;
            // For each statement S in the statement list in the block statement
            for (auto s : *stmt->stmt.block.stmt_list) {
                // Generate the LLVM IR for S by calling the genIRStmt subroutine recursively
                prevBB = genIRStmt(mod, s, builder, prevBB);
            }
            return prevBB;
        }
        default:
            fprintf(stderr, "Unknown statement type\n");
            exit(1);
    }
}

//Input: astNode of the expression, builder reference (this allows the subroutine to add LLVM 
//instructions in the correct basic block)
//Output: LLVMValueRef of the expression
LLVMValueRef genIRExpr(LLVMModuleRef mod, astNode* expr, LLVMBuilderRef builder) {
    printf("Generating IR for expression\n");
    switch (expr->type) {
        case ast_cnst:
            printf("Generating IR for constant\n");
            return LLVMConstInt(LLVMInt32Type(), expr->cnst.value, 0);
        case ast_var:
            printf("Generating IR for variable\n");
            return LLVMBuildLoad2(builder, LLVMInt32Type(), var_map[expr->var.name], "");
        case ast_uexpr: {
            printf("Generating IR for unary expression\n");
            LLVMValueRef operand = genIRExpr(mod, expr->uexpr.expr, builder);
            if (expr->uexpr.op == uminus) {
                return LLVMBuildNeg(builder, operand, "");
            }
            break;
        }
        // case of binary expressions 
        case ast_bexpr: {
            printf("Generating IR for binary expression\n");
            LLVMValueRef lhs = genIRExpr(mod, expr->bexpr.lhs, builder);
            LLVMValueRef rhs = genIRExpr(mod, expr->bexpr.rhs, builder);
            return createBinaryOp(builder, expr->bexpr.op, lhs, rhs); // calling helper function 
        }
        case ast_rexpr: {
            printf("Generating IR for relational expression\n");
            LLVMValueRef lhs = genIRExpr(mod, expr->rexpr.lhs, builder);
            LLVMValueRef rhs = genIRExpr(mod, expr->rexpr.rhs, builder);
            LLVMIntPredicate pred;
            switch (expr->rexpr.op) {
                case lt: pred = LLVMIntSLT; break;
                case gt: pred = LLVMIntSGT; break;
                case le: pred = LLVMIntSLE; break;
                case ge: pred = LLVMIntSGE; break;
                case eq: pred = LLVMIntEQ; break;
                case neq: pred = LLVMIntNE; break;
                default: assert(0 && "Unknown comparison operator");
            }
            return LLVMBuildICmp(builder, pred, lhs, rhs, "");
        }
        case ast_call:
            printf("Generating IR for function call expression\n");
            return LLVMBuildCall(builder, LLVMGetNamedFunction(mod, "read"), NULL, 0, "");
        default:
            fprintf(stderr, "Unknown expression type\n");
            exit(1);
    }
    return NULL;
}

// helper function for the genIRExpr that handles binary operations
LLVMValueRef createBinaryOp(LLVMBuilderRef builder, op_type op, LLVMValueRef lhs, LLVMValueRef rhs) {
    printf("Generating IR for binary operation\n");
    switch (op) {
        case add: return LLVMBuildAdd(builder, lhs, rhs, "add");
        case sub: return LLVMBuildSub(builder, lhs, rhs, "sub");
        case mul: return LLVMBuildMul(builder, lhs, rhs, "mul");
        case divide: return LLVMBuildSDiv(builder, lhs, rhs, "div");
        default: assert(0 && "Unknown binary operator");
    }
    return NULL;
}
