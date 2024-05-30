/*
*   Purpose: This file is the associated .h file for the semantic analysis step of part 1. 
*
*   Author: Carly Retterer
*   Date: 4/16/2024
*/ 

#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include <string>
#include <vector>
#include <stack>
#include "ast.h"

//using SymbolTable = vector<astNode*>;

//using SymbolTable = vector<std::string>;
/*
* C++ STL vector used as the primary data structure to store symbols. 
*/
using SymbolTable = vector<std::string>;

/**
 * 
 * This function traverses the AST nodes and performs semantic analysis. It helps to check 
 * to ensure that variables are declared before they are used and there is only one 
 * declaration of a variable in any given scope. 
 *
 * @param node Pointer to the current AST node being visited.
 * @param symbolTableStack Reference to the stack of symbol tables.
 * returns: boolean that is true if function runs successfully 
 */
bool visitNode(astNode* node, stack<SymbolTable>& symbolTableStack);


#endif