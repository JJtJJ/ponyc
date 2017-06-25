#ifndef INFER_H
#define INFER_H

#include <platform.h>
#include <string.h>
#include "../ast/ast.h"
#include "../ast/token.h"
#include "ponyassert.h"

PONY_EXTERN_C_BEGIN

bool infer_gen_args(ast_t* typeparams, ast_t* typeargs);

bool extract_type(const char* typeparam, ast_t* params, ast_t* positionalargs, ast_t** out_type);

bool extract_type_inner(const char* typeparam, ast_t* param, ast_t* args, ast_t** out_type);
bool extract_type_typeargs(const char* typeparam, ast_t* param, ast_t* args, ast_t** out_type);

bool transform_provides(ast_t* expected, ast_t** actual);
bool transform_inner(ast_t** typeargs, ast_t* actual_typeparams, ast_t* actual_typeargs);

const char *method_name(ast_t* typeparams, ast_t* call);

PONY_EXTERN_C_END

#endif
