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
            LLVMValueRef rhs = genIRExpr(mod, stmt->stmt.asgn.rhs, builder);
            LLVMValueRef lhs = var_map[stmt->stmt.asgn.lhs->var.name];
            LLVMBuildStore(builder, rhs, lhs);
            return startBB;
        }
        // call nodes
        case ast_call: {
            printf("Generating IR for function call\n");
            LLVMValueRef value = stmt->stmt.call.param ? genIRExpr(mod, stmt->stmt.call.param, builder) : NULL;
            LLVMBuildCall(builder, LLVMGetNamedFunction(mod, stmt->stmt.call.name), value ? &value : NULL, value ? 1 : 0, "");
            return startBB;
        }

        //while nodes
        case ast_while: {
            printf("Generating IR for while loop\n");
            LLVMBasicBlockRef condBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "cond");
            LLVMBasicBlockRef bodyBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "body");
            LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "end");

            LLVMBuildBr(builder, condBB);
            LLVMPositionBuilderAtEnd(builder, condBB);
            LLVMValueRef cond = genIRExpr(mod, stmt->stmt.whilen.cond, builder);
            LLVMBuildCondBr(builder, cond, bodyBB, endBB);

            LLVMPositionBuilderAtEnd(builder, bodyBB);
            genIRStmt(mod, stmt->stmt.whilen.body, builder, bodyBB);
            LLVMBuildBr(builder, condBB);

            LLVMPositionBuilderAtEnd(builder, endBB);
            return endBB;
        }
        // if nodes 
        case ast_if: {
            printf("Generating IR for if statement\n");
            LLVMBasicBlockRef trueBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "true");
            LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "false");
            LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "end");

            LLVMValueRef cond = genIRExpr(mod, stmt->stmt.ifn.cond, builder);
            LLVMBuildCondBr(builder, cond, trueBB, falseBB);

            LLVMPositionBuilderAtEnd(builder, trueBB);
            LLVMBasicBlockRef ifExitBB = genIRStmt(mod, stmt->stmt.ifn.if_body, builder, trueBB);
            LLVMBuildBr(builder, endBB);

            LLVMPositionBuilderAtEnd(builder, falseBB);
            if (stmt->stmt.ifn.else_body) {
                LLVMBasicBlockRef elseExitBB = genIRStmt(mod, stmt->stmt.ifn.else_body, builder, falseBB);
                LLVMPositionBuilderAtEnd(builder, elseExitBB);
            }
            LLVMBuildBr(builder, endBB);

            LLVMPositionBuilderAtEnd(builder, endBB);
            return endBB;
        }
        // return nodes
        case ast_ret: {
            printf("Generating IR for return statement\n");
            LLVMValueRef ret_val = genIRExpr(mod, stmt->stmt.ret.expr, builder);
            LLVMBuildStore(builder, ret_val, ret_ref);
            LLVMBuildBr(builder, retBB);
            return LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "after_ret");
        }
        // block statements 
        case ast_block: {
            printf("Generating IR for block statement\n");
            LLVMBasicBlockRef prevBB = startBB;
            for (auto stmt_node : *stmt->stmt.block.stmt_list) {
                prevBB = genIRStmt(mod, stmt_node, builder, prevBB);
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
