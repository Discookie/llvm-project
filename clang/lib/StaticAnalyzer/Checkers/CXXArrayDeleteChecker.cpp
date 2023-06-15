//=== CXXArrayDeleteChecker.cpp --------------------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// 

#include "AllocationState.h"
#include "InterCheckerAPI.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ParentMap.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Analysis/ProgramPoint.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/Lexer.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/CommonBugCategories.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerHelpers.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicExtent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExplodedGraph.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramState.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramStateTrait.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramState_Fwd.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/SVals.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/StoreRef.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/SymbolManager.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetOperations.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <climits>
#include <functional>
#include <optional>
#include <utility>

using namespace clang;
using namespace ento;
using namespace std::placeholders;

namespace {

class ArrayRefState {
  const Stmt *S;
  const CXXRecordDecl *RD;

  ArrayRefState(const Stmt *s, const CXXRecordDecl *rd)
      : S(s), RD(rd) {}
public:

  const Stmt *getStmt() const { return S; }
  const CXXRecordDecl *getRegionDecl() const { return RD; }

  bool operator==(const ArrayRefState &X) const {
    return S == X.S && RD == X.RD;
  }

  void Profile(llvm::FoldingSetNodeID &ID) const {
    ID.AddPointer(S);
    ID.AddPointer(RD);
  }

  LLVM_DUMP_METHOD void dump(raw_ostream &OS) const {
    if (!RD) {
      OS << "<untyped>";
    } else {
      RD->dump();
    }
  }

  LLVM_DUMP_METHOD void dump() const { dump(llvm::errs()); }

  static ArrayRefState getTyped(const Stmt *S, const CXXRecordDecl *RD) {
    return ArrayRefState(S, RD);
  };
};

}

REGISTER_MAP_WITH_PROGRAMSTATE(ArrayRegionState, SymbolRef, ArrayRefState)

namespace {

class CXXArrayDeleteChecker
    : public Checker<check::NewAllocator, check::PostCall> {
public:
  CheckerNameRef CheckName;

  void checkNewAllocator(const CXXAllocatorCall &Call, CheckerContext &C) const;
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;

  ProgramStateRef ArrayNew(CheckerContext &C, const CallEvent &Call, ProgramStateRef State) const;
  ProgramStateRef ArrayDelete(CheckerContext &C, const CallEvent &Call, ProgramStateRef State) const;

private:
  mutable std::unique_ptr<BugType> BT_CXXArrayDelete;

  void HandleCXXArrayDelete(CheckerContext &C,
                          SourceRange Range,
                          const Expr *DeallocExpr,
                          const ArrayRefState *RS, SymbolRef Sym,
                          const CXXRecordDecl &RegionDecl,
                          const CXXRecordDecl &DeallocatedDecl) const;
};

}

namespace {
  
class ArrayNewVisitor final : public BugReporterVisitor {
  SymbolRef Sym;
public:
  ArrayNewVisitor(SymbolRef S) : Sym(S) {}

  static void *getTag() {
    static int Tag = 0;
    return &Tag;
  }

  void Profile(llvm::FoldingSetNodeID &ID) const override {
    ID.AddPointer(getTag());
    ID.AddPointer(Sym);
  }

  PathDiagnosticPieceRef VisitNode(const ExplodedNode *N,
                                   BugReporterContext &BRC,
                                   PathSensitiveBugReport &BR) override;
};

}

PathDiagnosticPieceRef ArrayNewVisitor::VisitNode(const ExplodedNode *N,
                                                  BugReporterContext &BRC,
                                                  PathSensitiveBugReport &BR) {
  ProgramStateRef state = N->getState();
  ProgramStateRef statePrev = N->getFirstPred()->getState();

  const ArrayRefState *RSCurr = state->get<ArrayRegionState>(Sym);
  const ArrayRefState *RSPrev = statePrev->get<ArrayRegionState>(Sym);

  const Stmt *S = N->getStmtForDiagnostics();
  if (!S && !RSCurr)
    return nullptr;

  const LocationContext *CurrentLC = N->getLocationContext();

  StringRef Msg;
  std::unique_ptr<StackHintGeneratorForSymbol> StackHint = nullptr;
  SmallString<256> Buf;
  llvm::raw_svector_ostream OS(Buf);

  PrintingPolicy Policy = PrintingPolicy(BRC.getASTContext().getLangOpts());

  if (isa_and_nonnull<CallExpr, CXXNewExpr>(S) && RSCurr && !RSPrev) {
    OS << "Array of type ";
    if (RSCurr->getRegionDecl()) {
      RSCurr->getRegionDecl()->getNameForDiagnostic(OS, Policy, true);
    } else {
      OS << "<none>";
    }

    OS << " is allocated";

    StackHint = std::make_unique<StackHintGeneratorForSymbol>(
        Sym, "Returned allocated memory");

    PathDiagnosticLocation Pos;
    if (!S)
      return nullptr;

    Pos = PathDiagnosticLocation(S, BRC.getSourceManager(),
                                   CurrentLC);

    Msg = OS.str();

    auto P = std::make_shared<PathDiagnosticEventPiece>(Pos, Msg, true);
    BR.addCallStackHint(P, std::move(StackHint));
    return P;
  }

  return nullptr;
}

void CXXArrayDeleteChecker::HandleCXXArrayDelete(CheckerContext &C,
                          SourceRange Range,
                          const Expr *DeallocExpr,
                          const ArrayRefState *RS, SymbolRef Sym,
                          const CXXRecordDecl &RegionDecl,
                          const CXXRecordDecl &DeallocatedDecl) const {
  if (ExplodedNode *N = C.generateErrorNode()) {
    if (!BT_CXXArrayDelete)
      BT_CXXArrayDelete.reset(
          new BugType(CheckName,
                      "Mismatched C++ array delete", categories::MemoryError));

    SmallString<256> buf;
    llvm::raw_svector_ostream os(buf);

    // const Expr *AllocExpr = cast<Expr>(RS->getStmt());

    PrintingPolicy Policy = PrintingPolicy(C.getLangOpts());

    os << "Array of derived type ";

    RegionDecl.getNameForDiagnostic(os, Policy, true);

    os << " should not be deallocated under its base type ";
    
    DeallocatedDecl.getNameForDiagnostic(os, Policy, true);

    auto R = std::make_unique<PathSensitiveBugReport>(*BT_CXXArrayDelete,
                                                      os.str(), N);
    R->markInteresting(Sym);
    R->addRange(Range);
    R->addVisitor<ArrayNewVisitor>(Sym);
    C.emitReport(std::move(R));
  }
}

void CXXArrayDeleteChecker::checkNewAllocator(const CXXAllocatorCall &Call, CheckerContext &C) const {
  llvm::errs() << "New allocator called\n";
}

void CXXArrayDeleteChecker::checkPostCall(const CallEvent &Call, CheckerContext &C) const {
  llvm::errs() << "Post call called\n";
  if (C.wasInlined)
    return;
  if (!Call.getOriginExpr())
    return;
  if (!Call.getDecl() || !isa<FunctionDecl>(Call.getDecl()))
    return;


  OverloadedOperatorKind Kind = cast<FunctionDecl>(Call.getDecl())->getOverloadedOperator();
  if (Kind != OO_Array_New && Kind != OO_Array_Delete)
    return;

  ProgramStateRef State = C.getState();

  if (Kind == OO_Array_New) {
    State = ArrayNew(C, Call, State);
  } else {
    State = ArrayDelete(C, Call, State);
  }
  

  C.addTransition(State);
}

ProgramStateRef CXXArrayDeleteChecker::ArrayNew(CheckerContext &C, const CallEvent &Call, ProgramStateRef State) const {
  if (!State)
    return nullptr;

  const Expr *CE = Call.getOriginExpr();

  CE->dump();

  // We expect the malloc functions to return a pointer.
  if (!Loc::isLocType(CE->getType()))
    return nullptr;
    

  unsigned Count = C.blockCount();
  SValBuilder &svalBuilder = C.getSValBuilder();
  const LocationContext *LCtx = C.getPredecessor()->getLocationContext();
  DefinedSVal RetVal = svalBuilder.getConjuredHeapSymbolVal(CE, LCtx, Count)
      .castAs<DefinedSVal>();
  State = State->BindExpr(CE, C.getLocationContext(), RetVal);

  SymbolRef Sym = RetVal.getAsLocSymbol();
  assert(Sym);

  const PointerType *RegionType = dyn_cast_or_null<PointerType>(Sym->getType().getTypePtrOrNull());
  const CXXRecordDecl *RegionDecl = !RegionType ? nullptr : RegionType->getPointeeCXXRecordDecl();

  return State->set<ArrayRegionState>(Sym, ArrayRefState::getTyped(CE, RegionDecl));
}

ProgramStateRef CXXArrayDeleteChecker::ArrayDelete(CheckerContext &C, const CallEvent &Call, ProgramStateRef State) const {
  if (!State)
    return nullptr;

  if (Call.getNumArgs() == 0)
    return nullptr;

  const Expr *ArgExpr = Call.getArgExpr(0);

  ArgExpr->dump();

  SVal ArgVal = C.getSVal(ArgExpr);
  if (!isa<DefinedOrUnknownSVal>(ArgVal))
    return nullptr;
  DefinedOrUnknownSVal location = ArgVal.castAs<DefinedOrUnknownSVal>();

  // Check for null dereferences.
  if (!isa<Loc>(location))
    return nullptr;
    
  // FIXME: For now
  if (ArgVal.isUnknownOrUndef())
    return nullptr;

  const Expr *ParentExpr = Call.getOriginExpr();

  const MemRegion *R = ArgVal.getAsRegion();
  if (!R)
    return nullptr;

  R = R->StripCasts();
  if (isa<BlockDataRegion>(R))
    return nullptr;

  const SymbolicRegion *SrBase = dyn_cast<SymbolicRegion>(R->getBaseRegion());
  // Various cases could lead to non-symbol values here.
  // For now, ignore them.
  if (!SrBase)
    return nullptr;
    

  SymbolRef SymBase = SrBase->getSymbol();
  const ArrayRefState *RsBase = State->get<ArrayRegionState>(SymBase);
  if (!RsBase)
    return nullptr;
  const CXXRecordDecl *RegionDecl = RsBase->getRegionDecl();
    

  const PointerType *DeallocatedType = dyn_cast_or_null<PointerType>(ArgExpr->getType().getTypePtrOrNull());
  const CXXRecordDecl *DeallocatedDecl = !DeallocatedType ? nullptr : DeallocatedType->getPointeeCXXRecordDecl();

  // Give an error when we're freeing one of the bases
  if (RegionDecl && DeallocatedDecl &&
    !RegionDecl->forallBases([&DeallocatedDecl](const CXXRecordDecl *BaseDecl) {
                // llvm::errs() << "Base is"; BaseDecl->dump();
                return BaseDecl != DeallocatedDecl;
              })) {
    HandleCXXArrayDelete(C, ArgExpr->getSourceRange(), ParentExpr, RsBase, SymBase, *RegionDecl, *DeallocatedDecl);
  }

  return State->remove<ArrayRegionState>(SymBase);
}

void ento::registerCXXArrayDeleteChecker(CheckerManager &mgr) {
  CXXArrayDeleteChecker *checker = mgr.registerChecker<CXXArrayDeleteChecker>();
  checker->CheckName = mgr.getCurrentCheckerName();
}

bool ento::shouldRegisterCXXArrayDeleteChecker(const CheckerManager &mgr) { return true; }
