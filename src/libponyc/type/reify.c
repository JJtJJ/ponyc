#include "reify.h"
#include "subtype.h"
#include "viewpoint.h"
#include "assemble.h"
#include "alias.h"
#include "../ast/token.h"
#include "ponyassert.h"
#include <string.h>

static void reify_typeparamref(pass_opt_t* opt, ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_TYPEPARAMREF);
  pony_assert(ast_id(typeparam) == TK_TYPEPARAM);

  ast_t* ref_def = (ast_t*)ast_data(ast);
  ast_t* param_def = (ast_t*)ast_data(typeparam);

  pony_assert(ref_def != NULL);
  pony_assert(param_def != NULL);
  AST_GET_CHILDREN(ref_def, ref_name, ref_constraint);
  AST_GET_CHILDREN(param_def, param_name, param_constraint);

  if(ref_def != param_def)
  {
    if(ast_name(ref_name) == ast_name(param_name))
    {
      if((ast_id(param_constraint) != TK_TYPEPARAMREF) &&
        !is_subtype(ref_constraint, param_constraint, NULL, opt))
        return;
    } else {
      return;
    }
  }

  // Keep ephemerality.
  switch(ast_id(ast_childidx(ast, 2)))
  {
    case TK_EPHEMERAL:
      typearg = consume_type(typearg, TK_NONE);
      break;

    case TK_NONE:
      break;

    case TK_ALIASED:
      typearg = alias(typearg);
      break;

    default:
      pony_assert(0);
  }

  ast_replace(astp, typearg);
}

static void reify_arrow(ast_t** astp)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_ARROW);
  AST_GET_CHILDREN(ast, left, right);

  ast_t* r_left = left;
  ast_t* r_right = right;

  if(ast_id(left) == TK_ARROW)
  {
    AST_GET_CHILDREN(left, l_left, l_right);
    r_left = l_left;
    r_right = viewpoint_type(l_right, right);
  }

  ast_t* r_type = viewpoint_type(r_left, r_right);
  ast_replace(astp, r_type);
}

static void reify_reference(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_REFERENCE);

  const char* name = ast_name(ast_child(ast));

  sym_status_t status;
  ast_t* ref_def = ast_get(ast, name, &status);

  if(ref_def == NULL)
    return;

  ast_t* param_def = (ast_t*)ast_data(typeparam);
  pony_assert(param_def != NULL);

  if(ref_def != param_def)
    return;

  ast_setid(ast, TK_TYPEREF);
  ast_add(ast, ast_from(ast, TK_NONE));    // 1st child: package reference
  ast_append(ast, ast_from(ast, TK_NONE)); // 3rd child: type args
  ast_settype(ast, typearg);
}

static void reify_one(pass_opt_t* opt, ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    reify_one(opt, &child, typeparam, typearg);
    child = ast_sibling(child);
  }

  ast_t* type = ast_type(ast);

  if(type != NULL)
    reify_one(opt, &type, typeparam, typearg);

  switch(ast_id(ast))
  {
    case TK_TYPEPARAMREF:
      reify_typeparamref(opt, astp, typeparam, typearg);
      break;

    case TK_ARROW:
      reify_arrow(astp);
      break;

    case TK_REFERENCE:
      reify_reference(astp, typeparam, typearg);
      break;

    default: {}
  }
}

bool infer_gen_args(ast_t* typeparams, ast_t* typeargs)
{
  ast_t* call = ast_nearest(typeargs, TK_CALL);
  ast_t* positionalargs = ast_child(call);

  char *fname = "create";
  ast_t* ast = ast_get_case(typeparams, fname, SYM_NONE);
  ast_t* params = ast_childidx(ast, 3);

  ast_t* typeparam = ast_child(typeparams);

  while(typeparam != NULL)
  {
    //ast_print(params);
    //ast_print(positionalargs);
    const char *param_id = ast_name(ast_child(typeparam));
    ast_t* type = NULL;
    if (!extract_type(param_id, params, positionalargs, &type))
        return false;

    pony_assert(type != NULL);
    ast_printverbose(type);
    ast_append(typeargs, type);

    typeparam = ast_sibling(typeparam);
  }

  return true;
}

bool extract_type_typeargs(const char* typeparam, ast_t* param, ast_t* args, ast_t** out_type)
{
  pony_assert(
    (ast_id(param) == TK_TYPEARGS) ||
    (ast_id(param) == TK_NONE)
    );

  ast_t* ref = ast_child(param);
  ast_t* type = ast_child(args);
  while (ref != NULL)
  {
    if (extract_type_inner(typeparam, ref, type, out_type))
        return true;

    ref = ast_sibling(ref);
    type = ast_sibling(type);
  }

  return false;
}

bool extract_type_inner(const char* typeparam, ast_t* param, ast_t* args, ast_t** out_type)
{
  pony_assert(
    (ast_id(param) == TK_PARAM && ast_id(args) == TK_SEQ) ||
    (ast_id(param) == TK_TYPEARGS && ast_id(args) == TK_TYPEARGS) ||
    (ast_id(param) == TK_TYPEPARAMREF) ||
    (ast_id(param) == TK_NOMINAL) ||
    (ast_id(param) == TK_NONE)
    );

  ast_t* next_param;
  ast_t* next_arg;

  ast_print(param);
  ast_print(args);

  switch(ast_id(param))
  {
    case TK_PARAM:
      next_param = ast_childidx(param, 1);
      next_arg = ast_type(args);
      return extract_type_inner(typeparam, next_param, next_arg, out_type);
      break;
    case TK_TYPEARGS:
      return extract_type_typeargs(typeparam, param, args, out_type);
      break;
    case TK_TYPEPARAMREF:
      if (!strcmp(ast_name(ast_child(param)), typeparam))
      {
        *out_type = args;
        return true;
      }
      return false;
      break;
    case TK_NOMINAL:
      next_param = ast_childidx(param, 2);
      next_arg = ast_childidx(args, 2);
      return extract_type_inner(typeparam, next_param, next_arg, out_type);
      break;
    default:
      break;
  }

  return false;
}

bool extract_type(const char* typeparam, ast_t* params, ast_t* positionalargs, 
  ast_t** out_type)
{
  pony_assert(
    (ast_id(params) == TK_PARAMS) ||
    (ast_id(params) == TK_NONE)
    );

  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positionalargs);
  while(param != NULL)
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

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors,
  pass_opt_t* opt)
{
  pony_assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  pony_assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  size_t param_count = ast_childcount(typeparams);
  size_t arg_count = ast_childcount(typeargs);

  if(param_count == arg_count)
    return true;

  if(param_count < arg_count)
  {
    {
      ast_error(opt->check.errors, typeargs, "too many type arguments");
      ast_error_continue(opt->check.errors, typeparams, "definition is here");
    }

    return false;
  }

  // Pick up default type arguments if they exist.
  ast_setid(typeargs, TK_TYPEARGS);
  ast_t* typeparam = ast_childidx(typeparams, arg_count);

  while(typeparam != NULL)
  {
    ast_t* defarg = ast_childidx(typeparam, 2);

    if(ast_id(defarg) == TK_NONE)
      break;

    ast_append(typeargs, defarg);
    typeparam = ast_sibling(typeparam);
  }

  if (typeparam != NULL)
  {
    if (infer_gen_args(typeparams, typeargs))
        return true;

    // A missing type parameter went without being inferred
    if (errors)
    {
      ast_error(opt->check.errors, typeargs, "not enough type arguments");
      ast_error_continue(opt->check.errors, typeparams, "definition is here");
    }

    return false;
  }

  return true;
}

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt,
  bool duplicate)
{
  (void)opt;
  pony_assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  pony_assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  ast_t* r_ast;
  if(duplicate)
    r_ast = ast_dup(ast);
  else
    r_ast = ast;

  // Iterate pairwise through the typeparams and typeargs.
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    reify_one(opt, &r_ast, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  pony_assert(typeparam == NULL);
  pony_assert(typearg == NULL);
  return r_ast;
}

ast_t* reify_method_def(ast_t* ast, ast_t* typeparams, ast_t* typeargs,
  pass_opt_t* opt)
{
  (void)opt;
  switch(ast_id(ast))
  {
    case TK_FUN:
    case TK_BE:
    case TK_NEW:
      break;

    default:
      pony_assert(false);
  }

  // Remove the body AST to avoid duplicating it.
  ast_t* body = ast_childidx(ast, 6);
  ast_t* temp_body = ast_blank(TK_NONE);
  ast_swap(body, temp_body);

  ast_t* r_ast = reify(ast, typeparams, typeargs, opt, true);

  ast_swap(temp_body, body);
  ast_free_unattached(temp_body);

  return r_ast;
}

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors, pass_opt_t* opt)
{
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while(typeparam != NULL)
  {
    switch(ast_id(typearg))
    {
      case TK_NOMINAL:
      {
        ast_t* def = (ast_t*)ast_data(typearg);

        if(ast_id(def) == TK_STRUCT)
        {
          if(report_errors)
          {
            ast_error(opt->check.errors, typearg,
              "a struct cannot be used as a type argument");
          }

          return false;
        }
        break;
      }

      case TK_TYPEPARAMREF:
      {
        ast_t* def = (ast_t*)ast_data(typearg);

        if(def == typeparam)
        {
          typeparam = ast_sibling(typeparam);
          typearg = ast_sibling(typearg);
          continue;
        }
        break;
      }

      default: {}
    }

    // Reify the constraint.
    ast_t* constraint = ast_childidx(typeparam, 1);
    ast_t* r_constraint = reify(constraint, typeparams, typeargs, opt,
      true);

    // A bound type must be a subtype of the constraint.
    errorframe_t info = NULL;
    errorframe_t* infop = (report_errors ? &info : NULL);
    if(!is_subtype_constraint(typearg, r_constraint, infop, opt))
    {
      if(report_errors)
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, orig,
          "type argument is outside its constraint");
        ast_error_frame(&frame, typearg,
          "argument: %s", ast_print_type(typearg));
        ast_error_frame(&frame, typeparam,
          "constraint: %s", ast_print_type(r_constraint));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
      }

      ast_free_unattached(r_constraint);
      return false;
    }

    ast_free_unattached(r_constraint);

    // A constructable constraint can only be fulfilled by a concrete typearg.
    if(is_constructable(constraint) && !is_concrete(typearg))
    {
      if(report_errors)
      {
        ast_error(opt->check.errors, orig, "a constructable constraint can "
          "only be fulfilled by a concrete type argument");
        ast_error_continue(opt->check.errors, typearg, "argument: %s",
          ast_print_type(typearg));
        ast_error_continue(opt->check.errors, typeparam, "constraint: %s",
          ast_print_type(constraint));
      }

      return false;
    }

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  pony_assert(typeparam == NULL);
  pony_assert(typearg == NULL);
  return true;
}
