/*
*   Purpose: This is the preprocessor.h file. 
*   Author: Carly Retterer
*   Date: 30 May 2024
*/


#ifndef RENAME_VARIABLES_H
#define RENAME_VARIABLES_H

#include <map>
#include <string>
#include <sstream>
#include "ast.h"

// Global variable for renaming map and unique variable counter
extern std::map<std::string, std::string> var_rename_map;
extern int unique_var_counter;

// Function declaration for renaming variables in the AST
void rename_variables(astNode* node);

#endif // RENAME_VARIABLES_H
