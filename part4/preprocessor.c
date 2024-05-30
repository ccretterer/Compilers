/*
*   Purpose: This is the preprocessor file to help rename variables such that the llvm builder
*   can successfully build the LLVM code.
*   Author: Carly Retterer
*   Date: 30 May 2024
*/

#include "preprocessor.h"
#include "ast.h"
#include <map>
#include <string>
#include <cstring>
#include <cstdlib>  // For malloc and free

// Global variable rename map
std::map<std::string, std::string> var_rename_map;

// Function to rename variables in the AST
void rename_variables(astNode* node) {
    if (!node) return;

    switch (node->type) {
        case ast_var: {
            auto it = var_rename_map.find(node->var.name);
            if (it != var_rename_map.end()) {
                std::string new_name = it->second;
                char* new_name_cstr = (char*)malloc((new_name.length() + 1) * sizeof(char));
                if (new_name_cstr == nullptr) {
                    // Handle memory allocation failure
                    fprintf(stderr, "Memory allocation failed\n");
                    exit(EXIT_FAILURE);
                }
                strcpy(new_name_cstr, new_name.c_str());
                node->var.name = new_name_cstr;  // Update the pointer to the new name
            }
            break;
        }
        // iterating through tree
        case ast_stmt: {
            switch (node->stmt.type) {
                case ast_asgn:
                    rename_variables(node->stmt.asgn.lhs);
                    rename_variables(node->stmt.asgn.rhs);
                    break;
                case ast_call:
                    if (node->stmt.call.param) {
                        rename_variables(node->stmt.call.param);
                    }
                    break;
                case ast_while:
                    rename_variables(node->stmt.whilen.cond);
                    rename_variables(node->stmt.whilen.body);
                    break;
                case ast_if:
                    rename_variables(node->stmt.ifn.cond);
                    rename_variables(node->stmt.ifn.if_body);
                    if (node->stmt.ifn.else_body) {
                        rename_variables(node->stmt.ifn.else_body);
                    }
                    break;
                case ast_block: {
                    for (auto stmt_node : *(node->stmt.block.stmt_list)) {
                        rename_variables(stmt_node);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}
