//===- LoopVectorizer.cpp - Vectorize Loops  ----------------===//
//
//                     The Region Vectorizer
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "rv/transform/LoopVectorizer.h"
#include "rv/LinkAllPasses.h"

#include "rv/rv.h"
#include "rv/vectorMapping.h"
#include "rv/analysis/maskAnalysis.h"
#include "rv/region/LoopRegion.h"
#include "rv/region/Region.h"
#include "rv/sleefLibrary.h"
#include "rv/analysis/reductionAnalysis.h"
#include "rv/transform/remTransform.h"

#include "rvConfig.h"

#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Dominators.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"

#include "llvm/Transforms/Utils/Cloning.h"

#include "report.h"
#include <map>

#ifdef IF_DEBUG
#undef IF_DEBUG
#endif

#define IF_DEBUG if (true)

using namespace rv;
using namespace llvm;

// typedef DomTreeNodeBase<BasicBlock*> DomTreeNode;



bool LoopVectorizer::canVectorizeLoop(Loop &L) {
  if (!L.isAnnotatedParallel())
    return false;

  BasicBlock *ExitingBlock = L.getExitingBlock();
  if (!ExitingBlock)
    return false;

  if (!isa<BranchInst>(ExitingBlock->getTerminator()))
    return false;

  return true;
}

int
LoopVectorizer::getDependenceDistance(Loop & L) {
  int vectorWidth = getVectorWidth(L);
  if (vectorWidth > 0) return vectorWidth;

  if (!canVectorizeLoop(L)) return 1;
  return ParallelDistance; // fully parallel
}

int
LoopVectorizer::getTripAlignment(Loop & L) {
  int tripCount = getTripCount(L);
  if (tripCount > 0) return tripCount;
  return 1;
}


bool LoopVectorizer::canAdjustTripCount(Loop &L, int VectorWidth, int TripCount) {
  if (VectorWidth == TripCount)
    return true;

  return false;
}

int
LoopVectorizer::getTripCount(Loop &L) {
  auto *BTC = dyn_cast<SCEVConstant>(SE->getBackedgeTakenCount(&L));
  if (!BTC)
    return -1;

  int64_t BTCVal = BTC->getValue()->getSExtValue();
  if (BTCVal <= 1 || ((int64_t)((int) BTCVal)) != BTCVal)
    return -1;

  return BTCVal + 1;
}

int LoopVectorizer::getVectorWidth(Loop &L) {
  auto *LID = L.getLoopID();

  // try to recover from latch
  if (!LID) {
    auto * latch = L.getLoopLatch();
    LID = latch->getTerminator()->getMetadata("llvm.loop");
    if (LID) IF_DEBUG { errs() << "Recovered loop MD from latch!\n"; }
  }

  if (!LID) return -1;

  for (int i = 0, e = LID->getNumOperands(); i < e; i++) {
    const MDOperand &Op = LID->getOperand(i);
    auto *OpMD = dyn_cast<MDNode>(Op);
    if (!OpMD || OpMD->getNumOperands() != 2)
      continue;

    auto *Str = dyn_cast<MDString>(OpMD->getOperand(0));
    auto *Cst = dyn_cast<ConstantAsMetadata>(OpMD->getOperand(1));
    if (!Str || !Cst)
      continue;

    if (Str->getString().equals("llvm.loop.vectorize.enable")) {
      if (Cst->getValue()->isNullValue())
        return -1;
    }

    if (Str->getString().equals("llvm.loop.vectorize.width")) {
      if (auto *CstInt = dyn_cast<ConstantInt>(Cst->getValue()))
        return CstInt->getSExtValue();
    }
  }

  return -1;
}


Loop*
LoopVectorizer::transformToVectorizableLoop(Loop &L, int VectorWidth, int tripAlign, ValueSet & uniformOverrides) {
  IF_DEBUG { errs() << "\tCreating scalar remainder Loop for " << L.getName() << "\n"; }

  // try to applu the remainder transformation
  RemainderTransform remTrans(*F, *DT, *PDT, *LI, *reda);
  auto * preparedLoop = remTrans.createVectorizableLoop(L, uniformOverrides, VectorWidth, tripAlign);

  return preparedLoop;
}

bool
LoopVectorizer::vectorizeLoop(Loop &L) {
// check the dependence distance of this loop
  int depDist = getDependenceDistance(L);
  if (depDist <= 1) {
    Report() << "loopVecPass skip: won't vectorize " << L.getName() << " . Min dependence distance was " << depDist << "\n";
    return false;
  }

  //
  int tripAlign = getTripAlignment(L);

  int VectorWidth = getVectorWidth(L);
  if (VectorWidth < 0) {
    Report() << "loopVecPass skip: won't vectorize " << L.getName() << " . Vector width was " << VectorWidth << "\n";
    return false;
  }

  // if (tripAlign % VectorWidth != 0) {
  //   Report() << "loopVecPass skip: won't vectorize " << L.getName() << " . Could not adjust trip count\n";
  //   return false;
  // }

  // TODO check legality

  Report() << "loopVecPass: Vectorize " << L.getName() << " with VW: " << VectorWidth << " , Dependence Distance: " << depDist
         << " and TripAlignment: " << tripAlign << "\n";

  ValueSet uniOverrides;
  auto * PreparedLoop = transformToVectorizableLoop(L, VectorWidth, tripAlign, uniOverrides);
  if (!PreparedLoop) {
    Report() << "loopVecPass: Can not prepare vectorization of the loop\n";
    return false;
  }

  Module &M = *F->getParent();

// start vectorizing the prepared loop
  IF_DEBUG { errs() << "rv: Vectorizing loop " << L.getName() << "\n"; }

  VectorMapping targetMapping(F, F, VectorWidth);
  LoopRegion LoopRegionImpl(*PreparedLoop);
  Region LoopRegion(LoopRegionImpl);

  VectorizationInfo vecInfo(*F, VectorWidth, LoopRegion);


// Check reduction patterns of vector loop phis
  // configure initial shape for induction variable
  for (auto & inst : *PreparedLoop->getHeader()) {
    auto * phi = dyn_cast<PHINode>(&inst);
    if (!phi) continue;

    rv::Reduction * redInfo = reda->getReductionInfo(*phi);
    IF_DEBUG { errs() << "loopVecPass: header phi  " << *phi << " : "; }

    if (!redInfo) {
      errs() << "\n\tskip: non-reduction phi in vector loop header " << L.getName() << "\n";
      return false;
    }

    rv::VectorShape phiShape = redInfo->getShape(VectorWidth);

    IF_DEBUG{ redInfo->dump(); }
    IF_DEBUG { errs() << "header phi " << phi->getName() << " has shape " << phiShape.str() << "\n"; }

    vecInfo.setVectorShape(*phi, phiShape);
  }

  // set uniform overrides
  IF_DEBUG { errs() << "-- Setting remTrans uni overrides --\n"; }
  for (auto * val : uniOverrides) {
    IF_DEBUG { errs() << "- " << *val << "\n"; }
    vecInfo.setVectorShape(*val, VectorShape::uni());
  }

  //DT.verifyDomTree();
  //LI.verify(DT);

  IF_DEBUG {
    verifyFunction(*F, &errs());
    DT->verifyDomTree();
    PDT->print(errs());
    LI->print(errs());
    // LI->verify(*DT); // FIXME unreachable blocks
  }

  // Domin Frontier Graph
  DFG dfg(*DT);
  dfg.create(*F);

  // Control Dependence Graph
  CDG cdg(*PDT);
  cdg.create(*F);

// Vectorize
  // vectorizationAnalysis
  vectorizer->analyze(vecInfo, cdg, dfg, *LI, *PDT, *DT);

  IF_DEBUG F->dump();
  assert(L.getLoopPreheader());

  // mask analysis
  auto maskAnalysis = vectorizer->analyzeMasks(vecInfo, *LI);
  assert(maskAnalysis);
  IF_DEBUG { maskAnalysis->print(errs(), &M); }

  // mask generator
  bool genMaskOk = vectorizer->generateMasks(vecInfo, *maskAnalysis, *LI);
  if (!genMaskOk)
    llvm_unreachable("mask generation failed.");

  // control conversion
  bool linearizeOk =
      vectorizer->linearizeCFG(vecInfo, *maskAnalysis, *LI, *DT);
  if (!linearizeOk)
    llvm_unreachable("linearization failed.");

  const DominatorTree domTreeNew(
      *vecInfo.getMapping().scalarFn); // Control conversion does not preserve
                                       // the domTree so we have to rebuild it
                                       // for now

  // vectorize the prepared loop embedding it in its context
  bool vectorizeOk = vectorizer->vectorize(vecInfo, domTreeNew, *LI, *SE, *MDR, nullptr);
  if (!vectorizeOk)
    llvm_unreachable("vector code generation failed");

// restore analysis structures
  DT->recalculate(*F);
  PDT->recalculate(*F);

  return true;
}

bool LoopVectorizer::vectorizeLoopOrSubLoops(Loop &L) {
  if (vectorizeLoop(L))
    return true;

  bool Changed = false;
  for (Loop* SubL : L)
    Changed |= vectorizeLoopOrSubLoops(*SubL);

  return Changed;
}

bool LoopVectorizer::runOnFunction(Function &F) {
  if (getenv("RV_DISABLE")) return false;

  IF_DEBUG { errs() << " -- module before RV --\n"; F.getParent()->dump(); }

  Report() << "loopVecPass: run on " << F.getName() << "\n";
  bool Changed = false;

// stash function analyses
  this->F = &F;
  this->DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  this->PDT = &getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
  this->LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  this->SE = &getAnalysis<ScalarEvolutionWrapperPass>().getSE();
  this->MDR = &getAnalysis<MemoryDependenceWrapperPass>().getMemDep();

// setup PlatformInfo
  TargetTransformInfo & tti = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
  TargetLibraryInfo & tli = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  PlatformInfo platInfo(*F.getParent(), &tti, &tli);

  // TODO query target capabilities
  bool useSSE = false, useAVX = true, useAVX2 = true, useImpreciseFunctions = true;
  addSleefMappings(useSSE, useAVX, useAVX2, platInfo, useImpreciseFunctions);
  vectorizer.reset(new VectorizerInterface(platInfo));

  reda.reset(new ReductionAnalysis(F, *LI));
  reda->analyze();


  for (Loop *L : *LI)
    Changed |= vectorizeLoopOrSubLoops(*L);


  IF_DEBUG { errs() << " -- module after RV --\n"; F.getParent()->dump(); }

  // cleanup
  reda.reset();
  vectorizer.reset();
  this->F = nullptr;
  this->DT = nullptr;
  this->PDT = nullptr;
  this->LI = nullptr;
  this->SE = nullptr;
  this->MDR = nullptr;
  return Changed;
}

void LoopVectorizer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<MemoryDependenceWrapperPass>();
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();

  // PlatformInfo
  AU.addRequired<TargetTransformInfoWrapperPass>();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
}

char LoopVectorizer::ID = 0;

Pass *rv::createLoopVectorizerPass() { return new LoopVectorizer(); }

INITIALIZE_PASS_BEGIN(LoopVectorizer, "rv-loop-vectorize",
                      "RV - Vectorize loops", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MemoryDependenceWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
// PlatformInfo
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(LoopVectorizer, "rv-loop-vectorize", "RV - Vectorize loops",
                    false, false)
