#ifndef REIFY_H
#define REIFY_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool infer_gen_args(ast_t* typeparams, ast_t* typeargs);

bool extract_type_inner(const char* typeparam, ast_t* param, ast_t* args, ast_t** out_type);

bool extract_type(const char* typeparam, ast_t* params, ast_t* positionalargs, ast_t** out_type);

bool transform_provides(ast_t* expected, ast_t** actual);
bool transform_inner(ast_t** typeargs, ast_t* actual_typeparams, ast_t* actual_typeargs);

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors,
  pass_opt_t* opt);

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt,
  bool duplicate);

ast_t* reify_method_def(ast_t* ast, ast_t* typeparams, ast_t* typeargs,
  pass_opt_t* opt);

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
