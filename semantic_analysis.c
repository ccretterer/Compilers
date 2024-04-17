/* 
*   MiniC Compiler - Semantic Analysis
*
*   Purpose: This file is the implementation of the Semantic Analysis for the MiniC compiler. It runs through the AST and
*   to perform semantic checks and constuct a stack of symbol tables. The purpose of the semantic checks is to ensure proper scope
*   and declaration of variables. In other words, it ensures that the input file abides by two rules: a variable is declared before it is used
*   and there is only one declaration of the variable in a scope. 
*
*   Author: Carly Retterer
*   Date: 4/16/2024
*/

#include <stdio.h>
#include <vector>
#include <stack>
#include "ast.h"
#include "semantic_analysis.h"
#include <algorithm>

using SymbolTable = vector<std::string>;  // symbol table data structure 

bool visitNode(astNode* node, stack<SymbolTable>& symbolTableStack);

bool visitNode(astNode* node, stack<SymbolTable>& symbolTableStack){
    //checking if node passed is a null pointer
    if(node == nullptr){
        fprintf(stderr, "Error: node is null\n");
        return false;
    }
    // if the node is a block statement node and is body of a function: 
    // get the top symbol table from the stack and use it as the curr_sym_table 
    // visit all nodes in the statement list of block statement 
    // pop top of the stack
    if (node->type == ast_func) {
        if (node->func.body != nullptr && node->func.body->type == ast_stmt && node->func.body->stmt.type == ast_block) {
            astNode* blockNode = node->func.body;
            if (!symbolTableStack.empty()) {
                SymbolTable& curr_sym_table = symbolTableStack.top();
                if (blockNode->stmt.block.stmt_list != nullptr) {
                    for (astNode* stmt : *(blockNode->stmt.block.stmt_list)) {
                        // handling possible errors where the stmt is a null pointer
                        if (stmt == nullptr) {
                            fprintf(stderr, "Statement node is null\n");
                            continue;
                        }
                        visitNode(stmt, symbolTableStack);
                    }
                }
                symbolTableStack.pop();
            }
        }
    }
    // if the node is a block statement node: 
    // create a new symbol table curr_sym_table and push it to the symbol table stack
    // visit all nodes in the statement list of block statement 
    // pop top of the stack
    else if (node->type == ast_stmt && node->stmt.type == ast_block){
        SymbolTable curr_sym_table;
        symbolTableStack.push(curr_sym_table);
        if(node->stmt.block.stmt_list != NULL){
            for (astNode* stmt : *(node->stmt.block.stmt_list)) {
                // handling possible errors where the stmt is a null pointer
                if(stmt == nullptr){
                    fprintf(stderr, "Statement node is null.\n");
                }
                visitNode(stmt, symbolTableStack);
            }
        }
        symbolTableStack.pop();
    }

    // if the node is a function node: 
    // create a new symbol table curr_sym_table and push it to the symbol table stack
    // if func node has a parameter add parameter to curr_sym_table
    // visit the body node of the function node
    // pop top of the stack
    else if (node->type == ast_func) {
        SymbolTable curr_sym_table;
        symbolTableStack.push(curr_sym_table);
        if (node->func.param != nullptr) {
            curr_sym_table.push_back(node->func.param->var.name);
        }
        // checking to make sure function body is not a null pointer
        if (node->func.body == nullptr) {
            fprintf(stderr, "Function body node is null.\n");
        } else {
            visitNode(node->func.body, symbolTableStack);
        }
        symbolTableStack.pop();
    }

    // if the node is a declaration statement, check if the variable is in the symbol table 
    // at the top of the stack. If it does, then emit an error message. 
    // Otherwise, add the variable to the symbol table at the top of the stack.
    else if(node->type == ast_stmt && node->stmt.type == ast_decl){
        if (!symbolTableStack.empty()) {
            //find symbol table at top of stack
            SymbolTable& curr_table = symbolTableStack.top();
            char* declName = node->stmt.decl.name;
            // making sure declName is not null
            if(declName == NULL){
                fprintf(stderr, "Declaration name is null.\n");
            }
            //iterate through symbol table
            if (std::find(curr_table.begin(),curr_table.end(), declName) != curr_table.end()){
                printf("Error: Variable has already been declared.'%s'\n", declName); 
            }
            else{
                curr_table.push_back(declName);
            }
        }
    }

    // if the node is a variable node, check if it appears in one of the symbol tables on the stack. 
    // If it does not, then emit an error message with name of the variable.
    else if(node->type == ast_var) {
        char* varName = node->var.name;
        // checking to make sure variable name isn't null
        if(varName == NULL){
            fprintf(stderr, "varName is null\n");
        }
        bool ifFound = false;  // bool to keep track of whether or not variable name has beem foumd
        stack<SymbolTable> curr_stack = symbolTableStack;  //initializing empty stack and copying current stack to it
        while(!curr_stack.empty()) {
            SymbolTable& tempTable = curr_stack.top();
            // iterating through every symbol table in stack
            if(std::find(tempTable.begin(), tempTable.end(), varName) != tempTable.end()) {
                ifFound = true;
                break;
            }
            curr_stack.pop();
        }
        if (!ifFound) {
            printf("Error: Variable has not been declared. '%s'\n", varName);
        }
    }

    //for all other node types, visit all the child nodes of the current node
    else {
            // program nodes
        if (node->type == ast_prog) {
            // children
            visitNode(node->prog.ext1, symbolTableStack);
            visitNode(node->prog.ext2, symbolTableStack);
            visitNode(node->prog.func, symbolTableStack);
        }
        // statement nodes
        else if (node->type == ast_stmt) {
            switch (node->stmt.type) {
                case ast_call:
                    visitNode(node->stmt.call.param, symbolTableStack);
                    break;
                case ast_ret:
                    visitNode(node->stmt.ret.expr, symbolTableStack);
                    break;
                case ast_while:
                    visitNode(node->stmt.whilen.cond, symbolTableStack);
                    visitNode(node->stmt.whilen.body, symbolTableStack);
                    break;
                case ast_if:
                    visitNode(node->stmt.ifn.cond, symbolTableStack);
                    visitNode(node->stmt.ifn.if_body, symbolTableStack);
                    if (node->stmt.ifn.else_body != NULL) {
                        visitNode(node->stmt.ifn.else_body, symbolTableStack);
                    }
                    break;
                case ast_asgn:
                    visitNode(node->stmt.asgn.rhs, symbolTableStack);
                    visitNode(node->stmt.asgn.lhs, symbolTableStack);
                    break;
            }
        }
        // expr nodes
        else if (node->type == ast_rexpr) {
            visitNode(node->rexpr.lhs, symbolTableStack);
            visitNode(node->rexpr.rhs, symbolTableStack);
        }
        else if (node->type == ast_bexpr) {
            visitNode(node->bexpr.lhs, symbolTableStack);
            visitNode(node->bexpr.rhs, symbolTableStack);
        }
        else if (node->type == ast_uexpr) {
            visitNode(node->uexpr.expr, symbolTableStack);
        }
    }
    return true;
}