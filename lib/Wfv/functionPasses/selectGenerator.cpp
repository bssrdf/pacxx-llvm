/**
 * @file   selectGenerator.cpp
 * @date   06.06.2012
 * @author Ralf Karrenberg
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See the COPYING file in the root directory for details.
 *
 * Copyright (C) 2012 Saarland University
 *
 */

#include "wfv/functionPasses/selectGenerator.h"
#include "wfv/wfvInfo.h"
#include "wfv/analysis/maskAnalysis.h"
#include "wfv/analysis/loopLiveValueAnalysis.h"
#include "wfv/utils/metadata.h"
#include "wfv/utils/wfvTools.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h" // verifyFunction()
#include "llvm/Analysis/LoopInfo.h"

#include <stdexcept>

using namespace llvm;


char SelectGenerator::ID = 0;
// NOTE: The order of initialized dependencies is important
//       to prevent 'Unable to schedule' errors!
INITIALIZE_PASS_BEGIN(SelectGenerator, "selectGenerator", "SelectGenerator", false, false)
INITIALIZE_PASS_DEPENDENCY(WFVInfo)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MaskAnalysis)
INITIALIZE_PASS_DEPENDENCY(LoopLiveValueAnalysis)
INITIALIZE_PASS_END(SelectGenerator, "selectGenerator", "SelectGenerator", false, false)

// Public interface to the SelectGenerator pass
FunctionPass*
llvm::createSelectGeneratorPass()
{
	return new SelectGenerator();
}



SelectGenerator::SelectGenerator()
    : FunctionPass(ID),
    mInfo(nullptr),
    mLoopInfo(nullptr),
    mMaskAnalysis(nullptr),
    mLoopLiveValueAnalysis(nullptr)
{
    initializeSelectGeneratorPass(*PassRegistry::getPassRegistry());
}

SelectGenerator::~SelectGenerator()
{
    // Deleted by PassManager.
    mInfo                  = nullptr;
    mLoopInfo              = nullptr;
    mMaskAnalysis          = nullptr;
    mLoopLiveValueAnalysis = nullptr;
}

void
SelectGenerator::releaseMemory()
{
}

void
SelectGenerator::getAnalysisUsage(AnalysisUsage &AU) const
{
    AU.addRequired<WFVInfo>();
    AU.addPreserved<WFVInfo>();

    AU.addRequired<MaskAnalysis>();
    AU.addPreserved<MaskAnalysis>();

    AU.addRequired<LoopLiveValueAnalysis>();
    AU.addPreserved<LoopLiveValueAnalysis>();

    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();

    // Adding MaskGenerator here produces linking errors.
    // Following this up with adding
    // "extern char MaskGenerator::ID;"
    // produces runtime errors in PassManager.
    // Apparently we have to use INITIALIZE_PASS_DEPENDENCY only.
}

bool
SelectGenerator::doInitialization(Module& M)
{
    // The return value presumably signals whether the module was changed or not.
    // There is no documentation on this in LLVM.
    return false;
}

bool
SelectGenerator::doFinalization(Module& M)
{
    // The return value presumably signals whether the module was changed or not.
    // There is no documentation on this in LLVM.
    return false;
}

bool
SelectGenerator::runOnFunction(Function& F)
{
    mInfo                  = &getAnalysis<WFVInfo>();
    mLoopInfo              = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    mMaskAnalysis          = &getAnalysis<MaskAnalysis>();
    mLoopLiveValueAnalysis = &getAnalysis<LoopLiveValueAnalysis>();

    if(mInfo->mVerbose) {
        outs() << "\n#########################################################\n";
        outs() << "## SELECT GENERATION\n";
        outs() << "#########################################################\n";
    }

    // If an error occurred in one of the previous phases, abort.
    assert (mInfo->mFailure);
    if (*mInfo->mFailure) return false;

    if(!generatePhiSelects(&F)) {
        *mInfo->mFailure = true;
        return true;
    }
    if(!generateLoopSelects(&F)) {
        *mInfo->mFailure = true;
        return true;
    }

    if(mInfo->mVerbose) {
        outs() << "after select generation \n";
        F.print(outs());
    }

    return false;
}

void
SelectGenerator::print(raw_ostream& O, const Module* M) const
{
}

// Call 'generateSelectFromPhi()' for each non-uniform phi
// that is not the header of a loop.
bool
SelectGenerator::generatePhiSelects(Function* f)
{
    assert (f);

    // Loop over all blocks that contain a phi
    for (auto &BB : *f)
    {
        BasicBlock* block = &BB;

        // Ignore blocks without phis.
        if (block->getFirstNonPHI() == &block->getInstList().front())
        {
            continue;
        }

        // Ignore loop headers.
        if (Loop* loop = mLoopInfo->getLoopFor(block))
        {
            if (loop->getHeader() == block) continue;
        }

        // Ignore NON_DIVERGENT blocks.
        if (WFV::hasMetadata(block, WFV::WFV_METADATA_DIVERGENT_FALSE))
        {
            continue;
        }

        if(mInfo->mVerbose) {
            outs() << "\nGenerating selects for block '"
                   << block->getName() << "'...\n";
        }

        SmallPtrSet<PHINode*, 2> deleteSet;

        for (BasicBlock::iterator I=block->begin(), IE=block->end(); I!=IE; )
        {
            if (!isa<PHINode>(I)) break;
            PHINode* phi = cast<PHINode>(I++);

            // NOTE: There are cases where we have mask phis that *have* to
            //       be blended, e.g. if a branch condition depends upon the
            //       preceding control flow.
            //assert (!WFV::hasMetadata(phi, WFV::WFV_METADATA_MASK) &&
                    //"there is a mask phi where there should not be one");
            assert (!WFV::hasMetadata(phi, WFV::WFV_METADATA_OP_UNIFORM) &&
                    "phi in DIVERGENT block must not be uniform!");

            Value* blendedValue = generateSelectFromPhi(phi);

            if(!blendedValue)
                return false;

            // Update loop live value analysis.
            if (Instruction* blendedInst = dyn_cast<Instruction>(blendedValue))
            {
                mLoopLiveValueAnalysis->updateLiveValue(phi, blendedInst);
            }
            else
            {
                mLoopLiveValueAnalysis->removeLiveValue(phi);
            }

            deleteSet.insert(phi);
        }

        for (auto phi : deleteSet)
        {
            assert (phi->use_empty());
            phi->eraseFromParent();
        }

        if(mInfo->mVerbose) outs() << "Select-generation for block '"
                << block->getName() << "' finished.\n";
    }
    return true;
}

Value*
SelectGenerator::generateSelectFromPhi(PHINode* phi)
{
    assert (phi);
    assert (phi->getNumIncomingValues() >= 2 &&
            "Can not generate select from phi with less than two incoming values!");

    if(mInfo->mVerbose) outs() << "    generating select(s) for phi: " << *phi << "\n";

    BasicBlock* block = phi->getParent();
    assert (block);

    Value* blendedValue = phi->getIncomingValue(0);

    for (unsigned i=1, e=phi->getNumIncomingValues(); i<e; ++i)
    {
        Value*      incVal = phi->getIncomingValue(i);
        BasicBlock* incBB  = phi->getIncomingBlock(i);
        Value*      mask   = mMaskAnalysis->getExitMask(*incBB, *block);

        assert (incVal && incBB && mask);

        SelectInst* select = SelectInst::Create(mask, incVal, blendedValue, "", phi);
        WFV::copyMetadata(select, *phi);

        // Store where the values came from in metadata.
        // In case we have more than two incoming edges, the
        // "false" block of all selects after the first one is
        // nullptr since the value being blended is already a
        // composed one.
        BasicBlock* incBBFalse = i == 1 ?
            phi->getIncomingBlock(0) :
            nullptr;
        const bool isLast = i == e-1;
        WFV::setMetadataForBlend(select, incBB, incBBFalse, isLast);

        blendedValue = select;
        if(mInfo->mVerbose) outs() << "      generated select: " << *select << "\n";
    }

    phi->replaceAllUsesWith(blendedValue);

    if(mInfo->mVerbose) outs() << "    select-generation for phi finished.\n";

    return blendedValue;
}

bool
SelectGenerator::generateLoopSelects(Function* f)
{
    assert (f);

    if (mLoopInfo->empty()) return true;

    if(mInfo->mVerbose)
        outs() << "\nGenerating selects for loops of function " << f->getName() << "... \n";

    SmallPtrSet<SelectInst*, 8> selectSet;

    // Create loop selects for varying "top-level" loops.
    // Here, we can now stop recursion every time we see a uniform sub-loop
    // because possible varying loops nested deeper have been collected
    // before.
    for (auto &L : *mLoopInfo)
    {
        generateMultipleExitLoopSelects(L, selectSet);
    }

    if(mInfo->mVerbose) outs() << "\nGeneration of loop-selects finished!\n";

    // NOTE: We insert temporarily wrong code since the CFG has not
    //       been linearized yet. Thus, we must not verify the function.
    //DEBUG_WFV( verifyFunction(*f); );
    return true;
}

void
SelectGenerator::generateMultipleExitLoopSelects(Loop*                        loop,
                                                 SmallPtrSet<SelectInst*, 8>& selectSet)
{
    assert (loop);

    const bool isDivergentLoop = WFV::hasMetadata(loop, WFV::WFV_METADATA_LOOP_DIVERGENT_TRUE);

    // First collect loop live values.
    LoopLiveValueAnalysis::LoopLiveValueInfo& liveValues =
        *mLoopLiveValueAnalysis->getLiveValueInfo(loop);

    // We have to iterate through ALL loops, but only generate selects
    // for those that are DIVERGENT. Since we have to generate a few
    // things before recursing, this has to be prevented for non-
    // divergent loops.
    LoopResults* loopResults = nullptr;
    if (isDivergentLoop && !liveValues.empty())
    {
        // We need information about the parent loops' result phis during recursion.
        // Thus, generate them here already.
        loopResults = new LoopResults(loop);
        Instruction* pos = &*loop->getHeader()->getFirstInsertionPt();

        for (auto &liveValue : liveValues)
        {
            Type* type = liveValue->getType();
            PHINode* phi = PHINode::Create(type, 2U, "result.phi", pos);
            loopResults->addResultPhi(liveValue, phi);

            // A result phi is always OP_UNIFORM since it is in the loop header.
            // Also, it is always RES_VECTOR since it represents accumulated values
            // from different loop iterations, unless the incoming value requires
            // it to be OP_SEQUENTIAL/RES_SCALARS.
            // A live value may be uniform inside the loop but varying outside due
            // to the divergence of the loop. In this case, we have to make sure
            // that the result value is properly vectorized. The update operation
            // of the result vector will automatically broadcast the uniform value
            // and update only the appropriate elements of the result vector.
            if (WFV::hasMetadata(liveValue, WFV::WFV_METADATA_RES_SCALARS))
            {
                WFV::setMetadata(phi, WFV::WFV_METADATA_OP_SEQUENTIAL);
                WFV::setMetadata(phi, WFV::WFV_METADATA_RES_SCALARS);
            }
            else
            {
                WFV::setMetadata(phi, WFV::WFV_METADATA_OP_UNIFORM);
                WFV::setMetadata(phi, WFV::WFV_METADATA_RES_VECTOR);
            }
            WFV::setMetadata(phi, WFV::WFV_METADATA_INDEX_RANDOM);
            WFV::setMetadata(phi, WFV::WFV_METADATA_ALIGNED_FALSE);
        }

        mLoopLiveValueAnalysis->setLoopResults(loop, loopResults);
    }

    // Now recursively generate selects for nested loops.
    for (auto &SL : *loop)
    {
        generateMultipleExitLoopSelects(SL, selectSet);
    }

    // Ignore non-divergent loops.
    if (!isDivergentLoop)
    {
        if(mInfo->mVerbose) {
            outs() << "  Ignored non-divergent loop: ";
            outs() << loop->getHeader()->getName() << "\n";
        }
        return;
    }

    // If there are no live values (e.g. in a loop which only has side-effects),
    // don't generate any selects.
    if (liveValues.empty())
    {
        if(mInfo->mVerbose) {
            errs() << "  Loop has no result-values to generate selects for: ";
            errs() << loop->getHeader()->getName() << "\n";
        }
        return;
    }

    if(mInfo->mVerbose)
        outs() << "\n  Generating selects for divergent loop: " << loop->getHeader()->getName() << "\n";

    BasicBlock* latchBB = loop->getLoopLatch();

    // Generate combined exit mask (required for blending in latch).
    Value* combinedExitMask = mMaskAnalysis->getCombinedLoopExitMask(*loop);

    if(mInfo->mVerbose) outs() << "    combined exit mask: " << *combinedExitMask << "\n";


    if(mInfo->mVerbose) outs() << "  generating selects...\n";
    for (auto it=liveValues.begin(), E=liveValues.end(); it!=E; ++it)
    {
        Instruction* liveValue = *it;
        if(mInfo->mVerbose) outs() << "    generating select for live value: " << *liveValue << "\n";

        // Make sure we do not blend result vectors more than once.
        if (SelectInst* sel = dyn_cast<SelectInst>(liveValue))
        {
            if (selectSet.count(sel)) continue;
        }

        // condition = combined exit mask
        //             ('true' iff an instance left over any exit in the current iteration)
        // trueval   = keep old result (resultPhi)
        // falseval  = blend new result (instance left loop -> store result) (liveValue)
        if(mInfo->mVerbose) outs() << "      blend mask: " << *combinedExitMask << "\n";

        // Generate 'result-vector' for live value,
        // Consisting of phi in loop-header and select in latch
        // (to only blend once for all exits).

        // Get result phi.
        PHINode* resultPhi = loopResults->getResultPhi(liveValue);
        assert (resultPhi);

        assert (combinedExitMask && resultPhi && liveValue &&
                "required values for loop select generation must not be nullptr!");

        // Generate select
        // NOTE: We do not add loop selects to the selectInfoMap.
        SelectInst* resultSelect = SelectInst::Create(combinedExitMask,
                                                      liveValue,
                                                      resultPhi,
                                                      "result.vec",
                                                      latchBB->getTerminator());

        // A live value may be uniform inside the loop but varying outside due
        // to the divergence of the loop. In this case, we have to make sure
        // that the result value is properly vectorized. The update operation
        // of the result vector will automatically broadcast the uniform value
        // and update only the appropriate elements of the result vector.
        if (WFV::hasMetadata(liveValue, WFV::WFV_METADATA_RES_UNIFORM))
        {
            WFV::setMetadata(resultSelect, WFV::WFV_METADATA_RES_VECTOR);
            WFV::setMetadata(resultSelect, WFV::WFV_METADATA_OP_VARYING);
            WFV::setMetadata(resultSelect, WFV::WFV_METADATA_INDEX_RANDOM);
            WFV::setMetadata(resultSelect, WFV::WFV_METADATA_ALIGNED_FALSE);
        }
        else
        {
            WFV::copyMetadata(resultSelect, *liveValue);
        }
        WFV::setMetadataForBlend(resultSelect, latchBB, nullptr, true);

        if(mInfo->mVerbose) outs() << "      select generated: " << *resultSelect << "\n";

        selectSet.insert(resultSelect);
        loopResults->addResultSelect(liveValue, resultSelect);

        // If either of these two cases is true, the incoming value from the
        // loop preheader is undef (since there is no result yet, since the
        // value is only live for this loop and possibly inner loops, but not
        // for outer ones):
        // 1) The current loop is marked as the "top level divergent" one of
        //    this loop nest. This means that there is no outer loop, or all
        //    outer loops are non-divergent.
        // 2) The parent loop has no results or no result that corresponds to
        //    the current live value (= this live value only exits over edges
        //    that leave no outer loop).
        if (WFV::hasMetadata(loop, WFV::WFV_METADATA_LOOP_TOP_LEVEL_DIVERGENT) ||
            !hasLiveValueResult(loop->getParentLoop(), liveValue))
        {
            resultPhi->addIncoming(UndefValue::get(liveValue->getType()),
                                   loop->getLoopPreheader());
        }
        else
        {
            // Use result phi of this live value from parent loop
            assert (mLoopLiveValueAnalysis->hasLoopResults(loop->getParentLoop()));
            LoopResults* parentLoopResults =
                mLoopLiveValueAnalysis->getLoopResults(loop->getParentLoop());

            assert (!parentLoopResults->empty());
            assert (!parentLoopResults->hasResult(liveValue));

            PHINode* parentResultPhi = parentLoopResults->getResultPhi(liveValue);
            resultPhi->addIncoming(parentResultPhi, loop->getLoopPreheader());
        }

        resultPhi->addIncoming(resultSelect, latchBB);

        if(mInfo->mVerbose) outs() << "      result phi generated: " << *resultPhi << "\n";

        // If the live value's definition is in a child loop, the update operation of
        // this loop has to refer to the update operation of the next nested child loop
        // that is left together.
        const bool liveValueDefinedInNestedLoop =
            mLoopInfo->getLoopFor(liveValue->getParent()) != loop;

        if (liveValueDefinedInNestedLoop)
        {
            Loop* nextNestedLoop = WFV::findNestedLoopOfInst(loop, liveValue);

            Value* nestedVal = nullptr;
            if (!nextNestedLoop || !mLoopLiveValueAnalysis->hasLoopResults(nextNestedLoop))
            {
                // If we do not have any info on this nested loop stored,
                // it must be a uniform one.
                assert (WFV::hasMetadata(nextNestedLoop,
                                         WFV::WFV_METADATA_LOOP_DIVERGENT_FALSE));

                // Set live value operand to the live value itself (there
                // are no blending operations inside a uniform loop).
                nestedVal = liveValue;
            }
            else
            {
                // Otherwise, get the result select from the nested loop.
                LoopResults* nestedLoopResults =
                    mLoopLiveValueAnalysis->getLoopResults(nextNestedLoop);
                assert (!nestedLoopResults->empty()); // TODO: Could this happen?
                SelectInst* nestedSelect = nestedLoopResults->getResult(liveValue);
                nestedVal = nestedSelect;
            }
            assert (nestedVal);
            resultSelect->setOperand(1, nestedVal);
        }

        // Replace all uses of live values that are on the next outer nesting level.
        replaceDirectParentLoopUsesOfWith(liveValue, resultSelect, loop);

        // If this is a divergent "top level" loop, call replaceNonLoopUsesOfWith().
        if (WFV::hasMetadata(loop, WFV::WFV_METADATA_LOOP_TOP_LEVEL_DIVERGENT))
        {
            replaceNonLoopUsesOfWith(liveValue, resultSelect, loop);
        }
    }

    // Loops with mixed OPTIONAL and MANDATORY exits require special handling:
    // In each OPTIONAL exit block, we need to blend the value in the LCSSA phi
    // with the result phi using the corresponding exit mask.
    // This is required because otherwise, we have undef values for all
    // instances if the loop is left before the update operation was executed
    // at least once.
    SmallVector<BasicBlock*, 2> exitBlocks;
    loop->getExitBlocks(exitBlocks);
    const bool hasMultipleExits = exitBlocks.size() > 1;

    bool hasOptionalExit = false;
    for (auto &exitBB : exitBlocks)
    {
        if (!WFV::hasMetadata(exitBB, WFV::WFV_METADATA_OPTIONAL)) continue;
        hasOptionalExit = true;
        break;
    }

    if (!hasMultipleExits || !hasOptionalExit)
    {
        if(mInfo->mVerbose)
            outs() << "  generation of selects finished for divergent loop: " << loop->getHeader()->getName() << "\n";
        return;
    }

    for (auto it=liveValues.begin(), E=liveValues.end(); it!=E; ++it)
    {
        Instruction* liveValue = *it;
        if(mInfo->mVerbose) {
            outs() << "    generating optional-exit selects for live value: ";
            outs() << *liveValue << "\n";
        }

        // Get result phi that corresponds to this live value.
        SelectInst* resultVec = loopResults->getResult(liveValue);
        assert (resultVec);

        for (auto &exitBB : exitBlocks)
        {
            if (!WFV::hasMetadata(exitBB, WFV::WFV_METADATA_OPTIONAL)) continue;

            if(mInfo->mVerbose) {
                outs() << "      generating selects in OPTIONAL exit block '";
                outs() << exitBB->getName() << "'\n";
            }

            BasicBlock* exitingBB = exitBB->getUniquePredecessor();
            assert (exitingBB);

            // Get LCSSA value that corresponds to this live value.
            // The phi is the one that has the result vector as its incoming
            // value. This is because during generation of the result phi and
            // vector, we replace all uses of the live value by the vector.
            // In case of an OPTIONAL exit, we also revert this to have correct
            // LCSSA phis again.
            PHINode* origLCSSAPhi = nullptr;
            for (auto &I : *exitBB)
            {
                if (!isa<PHINode>(I)) break;
                PHINode* phi = cast<PHINode>(&I);
                Value*   val = phi->getIncomingValueForBlock(exitingBB);
                if (val != resultVec) continue;
                assert (!origLCSSAPhi && "expected only one LCSSA phi for the same live value!");
                origLCSSAPhi = phi;
            }

            // If no corresponding LCSSA phi was found on this exit, the
            // current live value is not live on this exit.
            if (!origLCSSAPhi)
            {
                if(mInfo->mVerbose) outs() << "        could not find LCSSA phi on this exit.\n";
                continue;
            }

            assert (WFV::hasMetadata(origLCSSAPhi, WFV::WFV_METADATA_OP_UNIFORM));
            assert (WFV::hasMetadata(origLCSSAPhi, WFV::WFV_METADATA_RES_VECTOR));

            // Convert to LCSSA by reverting the existing phi (live value as incoming value).
            // Generating a select is not necessary.
            origLCSSAPhi->setIncomingValue(origLCSSAPhi->getBasicBlockIndex(exitingBB), liveValue);
        }
    }

    if(mInfo->mVerbose)
        outs() << "  generation of selects finished for divergent loop: " << loop->getHeader()->getName() << "\n";
}

bool
SelectGenerator::hasLiveValueResult(const Loop* loop, const Instruction* liveValue) const
{
    assert (loop && liveValue);
    if (!mLoopLiveValueAnalysis->hasLoopResults(loop)) return false;
    const LoopResults* results = mLoopLiveValueAnalysis->getLoopResults(loop);
    return results->hasResult(liveValue);
}

void
SelectGenerator::replaceDirectParentLoopUsesOfWith(Value* oldValue, Value* newValue, Loop* loop)
{
    Loop* parentLoop = loop->getParentLoop();
    if (!parentLoop) return;
    for (Value::use_iterator U=oldValue->use_begin(),
        UE=oldValue->use_end(); U!=UE; )
    {
        Value* useVal = *U++;

        // Don't replace use in new value (also prevented by loop contains check).
        if (newValue == useVal) continue;

        assert (isa<Instruction>(useVal) && "all uses have to be instructions (!?)");
        Instruction* useI = cast<Instruction>(useVal);

        if (loop->contains(useI->getParent())) continue;
        if (!parentLoop->contains(useI->getParent())) continue;

        useI->replaceUsesOfWith(oldValue, newValue);
    }
}

void
SelectGenerator::replaceNonLoopUsesOfWith(Value* oldValue, Value* newValue, Loop* loop)
{
    assert (oldValue && newValue && loop);

    Loop* parentLoop = loop->getParentLoop();
    for (Value::use_iterator U=oldValue->use_begin(),
        UE=oldValue->use_end(); U!=UE; )
    {
        Value* useVal = *U++;

        // Don't replace use in new value (also prevented by loop contains check).
        if (newValue == useVal) continue;

        assert (isa<Instruction>(useVal) && "all uses have to be instructions (!?)");
        Instruction* useI = cast<Instruction>(useVal);

        // If the use is inside loop itself or some parent loop, ignore it.
        if (loop->contains(useI->getParent())) continue;
        if (parentLoop && isContainedInSomeParentLoop(useI->getParent(),
                                                      parentLoop))
        {
            continue;
        }

        useI->replaceUsesOfWith(oldValue, newValue);
    }
}

bool
SelectGenerator::isContainedInSomeParentLoop(BasicBlock* block, Loop* loop)
{
    assert (block && loop);
    if (loop->contains(block)) return true;

    Loop* parentLoop = loop->getParentLoop();

    return parentLoop ?
        isContainedInSomeParentLoop(block, parentLoop) :
        false;
}
