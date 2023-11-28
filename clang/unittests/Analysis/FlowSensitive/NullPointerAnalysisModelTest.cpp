//===- NullPointerAnalysisModelTest.cpp -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines a test for pointer nullability, specifically focused on
//  finding invalid dereferences, and unnecessary null-checks.
//  Only a limited set of operations are currently recognized. Notably, pointer
//  arithmetic, null-pointer assignments and _nullable/_nonnull attributes are
//  missing as of yet.
//
//  FIXME: Port over to the new type of dataflow test infrastructure
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/FlowSensitive/Models/NullPointerAnalysisModel.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/FlowSensitive/CFGMatchSwitch.h"
#include "clang/Analysis/FlowSensitive/DataflowAnalysis.h"
#include "clang/Analysis/FlowSensitive/DataflowEnvironment.h"
#include "clang/Analysis/FlowSensitive/DataflowLattice.h"
#include "clang/Analysis/FlowSensitive/MapLattice.h"
#include "clang/Analysis/FlowSensitive/NoopLattice.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Error.h"
#include "llvm/Testing/ADT/StringMapEntry.h"
#include "llvm/Testing/Support/Error.h"
#include "TestingSupport.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace clang::dataflow {
namespace {
using namespace ast_matchers;

constexpr char kCond[] = "condition";
constexpr char kVar[] = "var";
// constexpr char kKnown[] = "is-known";
constexpr char kIsNonnull[] = "is-nonnull";
constexpr char kIsNull[] = "is-null";

constexpr char kBoolTrue[] = "true";
constexpr char kBoolFalse[] = "false";
constexpr char kBoolInvalid[] = "invalid";
constexpr char kBoolUnknown[] = "unknown";
constexpr char kBoolNullptr[] = "is-nullptr";

std::string debugBoolValue(BoolValue *value, const Environment &Env) {
  if (value == nullptr) {
    return std::string(kBoolNullptr);
  } else {
    int boolState = 0;
    if (Env.proves(value->formula())) {
      boolState |= 1;
    }
    if (Env.proves(Env.makeNot(*value).formula())) {
      boolState |= 2;
    }
    switch (boolState) {
    case 0:
      return kBoolUnknown;
    case 1:
      return kBoolTrue;
    case 2:
      return kBoolFalse;
    // If both the condition and its negation are satisfied, the program point
    // is proven to be impossible.
    case 3:
      return kBoolInvalid;
    default:
      llvm_unreachable("all cases covered in switch");
    }
  }
}

auto nameToVar(llvm::StringRef name) {
  return declRefExpr(hasType(isAnyPointer()),
                     hasDeclaration(namedDecl(hasName(name))))
      .bind(kVar);
}

using ::clang::dataflow::test::AnalysisInputs;
using ::clang::dataflow::test::AnalysisOutputs;
using ::clang::dataflow::test::checkDataflow;
using ::llvm::IsStringMapEntry;
using ::testing::Args;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

MATCHER_P2(HasNullabilityState, null, nonnull,
           std::string("has nullability state where isNull is ") + null +
               " and isNonnull is " + nonnull) {
  return debugBoolValue(
             cast_or_null<BoolValue>(arg.first->getProperty(kIsNonnull)),
             *arg.second) == nonnull &&
         debugBoolValue(
             cast_or_null<BoolValue>(arg.first->getProperty(kIsNull)),
             *arg.second) == null;
}

MATCHER_P3(HoldsVariable, name, output, checks,
           ((negation ? "doesn't hold" : "holds") +
            llvm::StringRef(" a variable in its environment that ") +
            ::testing::DescribeMatcher<std::pair<Value *, Environment *>>(
                checks, negation))
               .str()) {
  auto MatchResults = match(functionDecl(hasDescendant(nameToVar(name))),
                            *output.Target, output.ASTCtx);
  assert(!MatchResults.empty());

  const auto *pointerExpr = MatchResults[0].template getNodeAs<Expr>(kVar);
  assert(pointerExpr != nullptr);

  const auto *ExprValue = arg.Env.getValue(*pointerExpr);

  if (ExprValue == nullptr) {
    return false;
  }

  return ExplainMatchResult(checks, std::pair{ExprValue, &arg.Env},
                            result_listener);
}

template <typename MatcherFactory>
void ExpectDataflowResult(llvm::StringRef Code, MatcherFactory Expectations) {
  // FIXME: This is a hack to initialize function parameters in the analysis.
  Environment *InitEnv;

  ASSERT_THAT_ERROR(
      checkDataflow<NullPointerAnalysisModel>(
          AnalysisInputs<NullPointerAnalysisModel>(
              Code, hasName("fun"),
              [&InitEnv](ASTContext &C, Environment &Env) {
                InitEnv = &Env;
                return NullPointerAnalysisModel(C);
              })
              .withSetupTest([&InitEnv](AnalysisOutputs &AO) {
                assert(InitEnv && "SetupTest ran at an unexpected time");
                NullPointerAnalysisModel &Analysis =
                    static_cast<NullPointerAnalysisModel &>(AO.Analysis);
                Analysis.initializeFunctionParameters(AO.CFCtx, *InitEnv);
                return llvm::Error::success();
              })
              .withASTBuildArgs({"-fsyntax-only", "-std=c++17"}),
          /*VerifyResults=*/[&Expectations](
              const llvm::StringMap<DataflowAnalysisState<
                  NullPointerAnalysisModel::Lattice>> &Results,
              const AnalysisOutputs &Output) {
            EXPECT_THAT(Results, Expectations(Output));
          }),
      llvm::Succeeded());
}

TEST(NullCheckAfterDereferenceTest, DebugOnly) {
  std::string Code = R"(
    typedef long size_t;
    extern void *malloc(size_t);
    extern int *ext();
    extern int *fncall();

    int fun(int *q, bool b) {
      int *p = (int*)malloc(sizeof(int));
      (void)0;
      // [[p_malloc]]

      if (p) {
        *p = 42;
        // [[p_true]]
      } else {
        (void)0;
        // [[p_false]]
      }

      (void)0;
      // [[p_merge]]

      p = nullptr;
      // [[p_nullptr]]

      p = ext();
      // [[p_extern]]

      return 0;
    }
  )";
  // FIXME: This is a hack to initialize function parameters in the analysis.
  Environment *InitEnv;

  ASSERT_THAT_ERROR(
    checkDataflow<NullPointerAnalysisModel>(
      AnalysisInputs<NullPointerAnalysisModel>(
        Code, hasName("fun"),
        [&InitEnv](ASTContext &C, Environment &Env) {
          InitEnv = &Env;
          return NullPointerAnalysisModel(C);
        })
        .withSetupTest([&InitEnv](AnalysisOutputs &AO) {
          assert(InitEnv && "SetupTest ran at an unexpected time");
          NullPointerAnalysisModel &Analysis = static_cast<NullPointerAnalysisModel&>(AO.Analysis);
          Analysis.initializeFunctionParameters(AO.CFCtx, *InitEnv);
          return llvm::Error::success();
        })
        .withASTBuildArgs({"-fsyntax-only", "-std=c++17"}),
      /*VerifyResults=*/
      [](const llvm::StringMap<DataflowAnalysisState<
                          NullPointerAnalysisModel::Lattice>> &Results,
                      const AnalysisOutputs &Output) {

          unsigned long long endbits = 0xffffff;

          int otherResultsCount = 0;
          for (const auto& a : Results) {

            llvm::errs() << a.first() << ":\n";
            a.second.Env.dump();
            
              auto MatchResults = 
                match(functionDecl(hasDescendant(nameToVar("p"))),
                      *Output.Target, Output.ASTCtx);
              assert(!MatchResults.empty());

              const auto *pointerExpr = MatchResults[0].getNodeAs<Expr>(kVar);
              assert(pointerExpr != nullptr);

              llvm::errs() << "ExprValue queried\n";
              pointerExpr->dump();

              const auto *ExprValue = a.second.Env.getValue(*pointerExpr);
              if (ExprValue == nullptr) {
                continue;
              }
              llvm::errs() << "Is not null\n";

              auto *NullptrValue = cast_or_null<BoolValue>(ExprValue->getProperty(kIsNull));
              auto *NonnullValue = cast_or_null<BoolValue>(ExprValue->getProperty(kIsNonnull));

              llvm::errs() << " -- 0: " << debugBoolValue(NullptrValue, a.second.Env)
                << ", Ø: "<< debugBoolValue(NonnullValue, a.second.Env) << "\n";
              llvm::errs() << " -- #0: " << ((unsigned long long)NullptrValue & endbits) << ", #Ø: "<< ((unsigned long long)NonnullValue & endbits) << "\n";
          }
      }),
      llvm::Succeeded());
}

TEST(NullCheckAfterDereferenceTest, DereferenceTypes) {
  std::string Code = R"(
    struct S {
      int a;
    };

    void fun(int *p, S *q) {
      *p = 0; // [[p]]

      q->a = 20; // [[q]]
    }
  )";
  ExpectDataflowResult(Code, [](const AnalysisOutputs &Output) -> auto {
    return UnorderedElementsAre(
        IsStringMapEntry(
            "p", HoldsVariable("p", Output,
                               HasNullabilityState(kBoolFalse, kBoolTrue))),
        IsStringMapEntry(
            "q", HoldsVariable("q", Output,
                               HasNullabilityState(kBoolFalse, kBoolTrue))));
  });
}

TEST(NullCheckAfterDereferenceTest, ConditionalTypes) {
  std::string Code = R"(
    void fun(int *p) {
      if (p) {
        (void)0; // [[p_true]]
      } else {
        (void)0; // [[p_false]]
      }

      // FIXME: Test ternary op
    }
  )";
  ExpectDataflowResult(Code, [](const AnalysisOutputs &Output) -> auto {
    return UnorderedElementsAre(
        IsStringMapEntry("p_true", HoldsVariable("p", Output,
                                                 HasNullabilityState(
                                                     kBoolFalse, kBoolTrue))),
        IsStringMapEntry("p_false", HoldsVariable("p", Output,
                                                  HasNullabilityState(
                                                      kBoolTrue, kBoolFalse))));
  });
}

TEST(NullCheckAfterDereferenceTest, UnrelatedCondition) {
  std::string Code = R"(
    void fun(int *p, bool b) {
      if (b) {
        *p = 42;
        (void)0; // [[p_b_true]]
      } else {
        (void)0; // [[p_b_false]]
      }

      (void)0; // [[p_merged]]

      if (b) {
        (void)0; // [[b_true]]

        if (p) {
          (void)0; // [[b_p_true]]
        } else {
          (void)0; // [[b_p_false]]
        }
      }
    }
  )";
  ExpectDataflowResult(Code, [](const AnalysisOutputs &Output) -> auto {
    return UnorderedElementsAre(
        IsStringMapEntry("p_b_true", HoldsVariable("p", Output,
                                                   HasNullabilityState(
                                                       kBoolFalse, kBoolTrue))),
        IsStringMapEntry(
            "p_b_false",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolNullptr, kBoolNullptr))),
        IsStringMapEntry(
            "p_merged",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolUnknown, kBoolUnknown))),
        IsStringMapEntry("b_true", HoldsVariable("p", Output,
                                                 HasNullabilityState(
                                                     kBoolFalse, kBoolTrue))),
        IsStringMapEntry("b_p_true", HoldsVariable("p", Output,
                                                   HasNullabilityState(
                                                       kBoolFalse,
                                                       kBoolTrue))),
        // FIXME: Flow condition is false in this last entry,
        // should test that instead of an invalid state
        IsStringMapEntry(
            "b_p_false",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolTrue, kBoolTrue))));
  });
}

TEST(NullCheckAfterDereferenceTest, AssignmentOfCommonValues) {
  std::string Code = R"(
    using size_t = decltype(sizeof(void*));
    extern void *malloc(size_t);
    extern int *ext();

    void fun() {
      int *p = (int*)malloc(sizeof(int));
      (void)0; // [[p_malloc]]

      if (p) {
        *p = 42; // [[p_true]]
      } else {
        (void)0; // [[p_false]]
      }

      (void)0; // [[p_merge]]

      p = nullptr; // [[p_nullptr]]

      p = ext(); // [[p_extern]]
    }
  )";
  ExpectDataflowResult(Code, [](const AnalysisOutputs &Output) -> auto {
    return UnorderedElementsAre(
        // FIXME: Recognize that malloc (and other functions) are nullable
        IsStringMapEntry(
            "p_malloc", 
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolUnknown, kBoolUnknown))),
        IsStringMapEntry(
            "p_true",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolFalse, kBoolTrue))),
        IsStringMapEntry(
            "p_false",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolTrue, kBoolFalse))),
        IsStringMapEntry(
            "p_merge",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolUnknown, kBoolUnknown))),
        IsStringMapEntry(
            "p_nullptr",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolTrue, kBoolFalse))),
        IsStringMapEntry(
            "p_extern",
            HoldsVariable("p", Output,
                          HasNullabilityState(kBoolUnknown, kBoolUnknown))));
  });
}

TEST(NullCheckAfterDereferenceTest, MergeValues) {
  std::string Code = R"(
    using size_t = decltype(sizeof(void*));
    extern void *malloc(size_t);

    void fun(int *p, bool b) {
      if (p) {
        *p = 10;
      } else {
        p = (int*)malloc(sizeof(int));
      }

      (void)0; // [[p_merge]]
    }
  )";
  ExpectDataflowResult(Code, [](const AnalysisOutputs &Output) -> auto {
    return UnorderedElementsAre(IsStringMapEntry(
        "p_merge",
        // Even if a pointer was nonnull on a branch, it is worth keeping the
        // more complex formula for more precise analysis.
        HoldsVariable("p", Output,
                      HasNullabilityState(kBoolUnknown, kBoolUnknown))));
  });
}

} // namespace
} // namespace clang::dataflow
