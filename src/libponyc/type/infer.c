#include "infer.h"

bool infer_gen_args(ast_t* typeparams, ast_t* typeargs)
{
  // Get the call definition
  ast_t* call = ast_nearest(typeparams, TK_CALL);
  if(call == NULL)
  {
    call = ast_nearest(typeargs, TK_CALL);
  }

  if(call == NULL)
    return false;

  ast_t* positionalargs = ast_child(call);

  ast_t* params = NULL;
  bool is_method = ast_id(ast_parent(typeparams)) == TK_FUNTYPE;

  // Get method parameters
  if(is_method)
  {
    params = ast_sibling(typeparams);
  }
  else
  {
    const char *fname = method_name(typeparams, call);
    ast_t* method_ast = ast_get_case(typeparams, fname, SYM_NONE);
    if(ast_id(method_ast) != TK_NEW)
      return false;

    params = ast_childidx(method_ast, 3);
  }

  ast_t* typeparam = ast_child(typeparams);

  // For each type parameter, attempt to extract a type for that
  // parameter from the arguments
  while(typeparam != NULL)
  {
    const char *param_id = ast_name(ast_child(typeparam));
    ast_t* type = NULL;
    if (!extract_type(param_id, params, positionalargs, &type))
        return false;

    pony_assert(type != NULL);
    ast_append(typeargs, type);

    typeparam = ast_sibling(typeparam);
  }

  return true;
}

/**
 * Extract the type of typeparam from the formal parameters and the arguments to the
 * constructor or method
 */
bool extract_type(const char* typeparam, ast_t* params, ast_t* positionalargs, 
  ast_t** out_type)
{
  pony_assert(
    (ast_id(params) == TK_PARAMS) ||
    (ast_id(params) == TK_TYPEPARAMS) ||
    (ast_id(params) == TK_NONE)
    );

  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positionalargs);
  while(param != NULL && arg != NULL)
  {
    if (extract_type_inner(typeparam, param, arg, out_type))
      break;

    param = ast_sibling(param);
    arg = ast_sibling(arg);
  }
  if (param == NULL)
    return false;

  return *out_type != NULL;
}

bool extract_type_inner(const char* typeparam, ast_t* param, ast_t* args, ast_t** out_type)
{
  pony_assert(
    (ast_id(param) == TK_PARAM && ast_id(args) == TK_SEQ) ||
    (ast_id(param) == TK_TYPEARGS && ast_id(args) == TK_TYPEARGS) ||
    (ast_id(param) == TK_TYPEPARAM) ||
    (ast_id(param) == TK_TYPEPARAMREF) ||
    (ast_id(param) == TK_NOMINAL) ||
    (ast_id(param) == TK_NONE)
    );

  ast_t* next_param;
  ast_t* next_arg;
  ast_t* arg_dup;

  switch(ast_id(param))
  {
    case TK_PARAM:
      next_param = ast_childidx(param, 1);
      next_arg = ast_type(args);
      if(next_param != NULL && next_arg != NULL)
        return extract_type_inner(typeparam, next_param, next_arg, out_type);
      break;
    case TK_TYPEARGS:
      return extract_type_typeargs(typeparam, param, args, out_type);
      break;
    case TK_TYPEPARAM:
    case TK_TYPEPARAMREF:
      if (!strcmp(ast_name(ast_child(param)), typeparam))
      {
        *out_type = ast_dup(args);
        return true;
      }
      break;
    case TK_NOMINAL:
      arg_dup = ast_dup(args);
      if(ast_id(arg_dup) == TK_NOMINAL)
      {
        if(!transform_provides(param, &arg_dup))
        {
          ast_free(arg_dup);
          return false;
        }
      }
      next_param = ast_childidx(param, 2);
      next_arg = ast_childidx(arg_dup, 2);
      if(next_param != NULL && next_arg != NULL)
      {
        bool res = extract_type_inner(typeparam, next_param, next_arg, out_type);
        ast_free(arg_dup);
        return res;
      }
      break;
    default:
      break;

  }

  return false;
}

bool extract_type_typeargs(const char* typeparam, ast_t* param, ast_t* args, ast_t** out_type)
{
  pony_assert(
    (ast_id(param) == TK_TYPEARGS) ||
    (ast_id(param) == TK_NONE)
    );

  ast_t* ref = ast_child(param);
  ast_t* type = ast_child(args);
  while (ref != NULL && type != NULL)
  {
    if(extract_type_inner(typeparam, ref, type, out_type))
        return true;

    ref = ast_sibling(ref);
    type = ast_sibling(type);
  }

  return false;
}


/**
 * Transform the actual given type to match the shape of the expected type
 */
bool transform_provides(ast_t* expected, ast_t** actual)
{
  pony_assert(
    (ast_id(expected) == TK_NOMINAL) &&
    (ast_id(*actual) == TK_NOMINAL)
    );

  const char* ex_name = ast_name(ast_childidx(expected, 1));
  const char* act_name = ast_name(ast_childidx(*actual, 1));

  if(!strcmp(ex_name, act_name))
    return true;

  ast_t* act_typeargs = ast_childidx(*actual, 2);
  ast_t* act_type_def = ast_get_case(expected, act_name, SYM_NONE);
  if(act_type_def == NULL)
    return false;

  ast_t* provide = ast_child(ast_childidx(act_type_def, 3));
  while(provide != NULL)
  {
    if(!strcmp(ast_name(ast_childidx(provide, 1)), ex_name))
    {
      ast_t* actual_typeparams = ast_childidx(act_type_def, 1);
      ast_t* provides_typeargs = ast_childidx(provide, 2);
      ast_t* new_typeargs = ast_dup(provides_typeargs);

      if(!transform_inner(&new_typeargs, actual_typeparams, act_typeargs))
      {
        ast_free(new_typeargs);
        return false;
      }

      ast_t* name_ast = ast_childidx(*actual, 1);
      ast_replace(&name_ast, ast_childidx(expected, 1));
      ast_replace(&act_typeargs, new_typeargs);
      return true;
    }
    else
    {
      provide = ast_sibling(provide);
    }
  }

  return false;
}

bool transform_inner(ast_t** typeargs, ast_t* actual_typeparams, ast_t* actual_typeargs)
{
  if(ast_id(*typeargs) == TK_TYPEPARAMREF)
  {
    const char* typename = ast_name(ast_child(*typeargs));
    ast_t* type = NULL;

    if(!extract_type(typename, actual_typeparams, actual_typeargs, &type))
      return false;

    ast_replace(typeargs, type);
    return true;
  }
  
  ast_t* child = ast_child(*typeargs);
  while (child != NULL)
  {
    if(!transform_inner(&child, actual_typeparams, actual_typeargs))
      return false;
    
    child = ast_sibling(child);
  }
  return true;
}

const char *method_name(ast_t* typeparams, ast_t* call)
{
  ast_t* parent = ast_parent(typeparams);
  if(parent == NULL)
    return NULL;

  ast_t* ctx = ast_childidx(call, 2);

  switch(ast_id(parent))
  {
    case TK_CLASS:
      switch(ast_id(ctx))
      {
        case TK_TYPEREF:
          return "create";
        case TK_DOT:
          return ast_name(ast_childidx(ctx, 1));
        default:
          return NULL;
      }
    case TK_FUNTYPE:
      return ast_name(ast_childidx(ast_parent(parent), 1));
      break;
    default:
      return NULL;
  }
}
