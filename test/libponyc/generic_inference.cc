#include <gtest/gtest.h>
#include <platform.h>

#include <reach/reach.h>

#include "util.h"

#ifdef _MSC_VER
// Stop MSVC from complaining about conversions from LLVMBool to bool.
# pragma warning(disable:4800)
#endif

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class GenericInferenceTest : public PassTest
{};


TEST_F(GenericInferenceTest, MissingConstructorParameterInferred)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a = A(U32(52))\n"

    "class A[T]\n"
    "  new create(t: T) => None";

  TEST_COMPILE(src);
}

TEST_F(GenericInferenceTest, TwoMissingConstructorParameters)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a = A(U32(52), true)\n"

    "class A[S, T]\n"
    "  new create(s: S, t: T) => None";

  TEST_COMPILE(src);
}

TEST_F(GenericInferenceTest, ClassAsMissingConstructorParameter)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a = A(U32(52))\n"
    "    var b = A(a)\n"

    "class A[T]\n"
    "  new create(t: T) => None";

  TEST_COMPILE(src);
}

TEST_F(GenericInferenceTest, ExtractMissingTypeParameterFromArgument)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a = A(U32(52))\n"
    "    var b = B(a)\n"

    "class A[T]\n"
    "  new create(t: T) => None\n"
    
    "class B[T]\n"
    "  new create(a: A[T]) => None";

  TEST_COMPILE(src);
}

TEST_F(GenericInferenceTest, MissingAlternativeConstructorParameter)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a = A.foo(U32(52))\n"

    "class A[T]\n"
    "  new foo(t: T) => None";

  TEST_COMPILE(src);
}

TEST_F(GenericInferenceTest, UnorderedConstructorArguments)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a = A(U32(52), 0, true)\n"

    "class A[S, T]\n"
    "  new create(t: T, i: U32, s: S) => None";

  TEST_COMPILE(src);
}

TEST_F(GenericInferenceTest, InferMissingMethodTypeParameters)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo(U32(52))"
    
    "  fun foo[T](t: T) => None";

  TEST_COMPILE(src);
}

TEST_F(GenericInferenceTest, InferTwoMissingMethodTypeParameters)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo(true, U32(52))"
    
    "  fun foo[S, T](s: S, t: T) => None";

  TEST_COMPILE(src);
}
