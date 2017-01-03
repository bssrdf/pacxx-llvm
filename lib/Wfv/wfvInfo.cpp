/**
 * @file   wfvInfo.cpp
 * @date   31.01.2012
 * @author Ralf Karrenberg
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See the COPYING file in the root directory for details.
 *
 * Copyright (C) 2012 Saarland University
 *
 */

#include "wfv/wfvInfo.h"
#include "wfv/utils/wfvTools.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Bitcode/BitcodeReader.h" // ParseBitcodeFile
#include "llvm/Bitcode/BitcodeWriter.h" // ParseBitcodeFile
#include "llvm/Support/MemoryBuffer.h" // MemoryBuffer
#include "llvm/Linker/Linker.h"               // Linker
#include "llvm/IR/Intrinsics.h"

#include <stdexcept>

using namespace llvm;


char WFVInfo::ID = 0;
INITIALIZE_PASS(WFVInfo, "wfvinfo", "WFV Info", false, true)


WFVInfo::WFVInfo()
: ImmutablePass(ID),
        mModule(nullptr),
        mContext(nullptr),
        mDataLayout(nullptr),
        mScalarFunction(nullptr),
        mSimdFunction(nullptr),
        mTTI(nullptr),
        mVectorizationFactor(0),
        mMaskPosition(-1),
        mDisableMemAccessAnalysis(false),
        mDisableControlFlowDivAnalysis(false),
        mDisableAllAnalyses(false),
        mInitialized(false),
        mPacxx(false),
        mVerbose(false),
        mFailure(nullptr),
        mTimerGroup(nullptr),
        mIsAnalyzed(false),
        mWFVLibLinked(false)
{
    assert (false && "must never call default constructor!");
}

WFVInfo::WFVInfo(Module*         M,
				 LLVMContext*    C,
				 Function* scalarFunction,
				 Function*       simdFunction,
                 TargetTransformInfo *TTI,
				 const unsigned  vectorizationFactor,
				 const int       maskPosition,
				 const bool      disableMemAccessAnalysis,
				 const bool      disableControlFlowDivAnalysis,
				 const bool      disableAllAnalyses,
                 const bool      pacxx,
				 const bool      verbose,
                 TimerGroup*     timerGroup)
        : ImmutablePass(ID),
        mModule(M),
        mContext(C),
        mDataLayout(new DataLayout(M)),
        mScalarFunction(scalarFunction),
        mSimdFunction(simdFunction),
        mTTI(TTI),
        mVectorizationFactor(vectorizationFactor),
        mMaskPosition(maskPosition),
        mDisableMemAccessAnalysis(disableMemAccessAnalysis),
        mDisableControlFlowDivAnalysis(disableControlFlowDivAnalysis),
        mDisableAllAnalyses(disableAllAnalyses),
        mInitialized(false),
        mPacxx(pacxx),
        mVerbose(verbose),
        mFailure(nullptr),
        mTimerGroup(timerGroup),
        mIsAnalyzed(false),
        mWFVLibLinked(false)
{
    // WFVInterface should set disableMemAccessAnalysis and disableControlFlowDivAnalysis
    // to 'false' if disableAllAnalyses is set.
    assert ((!mDisableAllAnalyses ||
             (mDisableMemAccessAnalysis && mDisableControlFlowDivAnalysis)) &&
            "expecting all analyses to internally be disabled if 'disableAllAnalyses' is set!");
    initializeWFVInfoPass(*PassRegistry::getPassRegistry());
    mInitialized = initialize();
}

WFVInfo::WFVInfo(const WFVInfo& other)
        : ImmutablePass(ID),
        mModule(other.mModule),
        mContext(other.mContext),
        mDataLayout(other.mDataLayout),
        mScalarFunction(other.mScalarFunction),
        mSimdFunction(other.mSimdFunction),
        mTTI(other.mTTI),
        mValueInfoMap(other.mValueInfoMap),
        mFunctionInfoMap(other.mFunctionInfoMap),
        mVectorizationFactor(other.mVectorizationFactor),
        mMaskPosition(other.mMaskPosition),
        mDisableMemAccessAnalysis(other.mDisableMemAccessAnalysis),
        mDisableControlFlowDivAnalysis(other.mDisableControlFlowDivAnalysis),
        mDisableAllAnalyses(other.mDisableAllAnalyses),
        mInitialized(other.mInitialized),
        mPacxx(other.mPacxx),
        mVerbose(other.mVerbose),
        mFailure(other.mFailure),
        mTimerGroup(other.mTimerGroup),
        mIsAnalyzed(other.mIsAnalyzed),
        mWFVLibLinked(other.mWFVLibLinked)
{
    initializeWFVInfoPass(*PassRegistry::getPassRegistry());
    mInitialized = initialize();

}

WFVInfo::~WFVInfo()
{
}

void
WFVInfo::releaseMemory()
{
    delete mDataLayout;
}

void
WFVInfo::getAnalysisUsage(AnalysisUsage &AU) const
{
    AU.setPreservesAll();
}

bool
WFVInfo::runOnModule(Module& M)
{
    assert (&M == mModule);
    return false;
}

bool
WFVInfo::initialize()
{
    if(mVectorizationFactor < 1) {
        errs() << "ERROR: Vectorization factor must be lager than zero\n";
        return false;
    }

    if(mMaskPosition != -1) {
        if (mMaskPosition < -1) {
            errs() << "ERROR: Mask position must be >= -1\n";
            return false;
        }


        if (mMaskPosition > (int) mSimdFunction->getFunctionType()->getNumParams()) {
            errs() << "ERROR: Mask position is larger then the number of parameters\n";
            return false;
        }

        Function::const_arg_iterator A = mScalarFunction->arg_begin();
        std::advance(A, mMaskPosition);
        if (!A->getType()->isIntegerTy(1)) {
            errs() << "ERROR: Mask argument at position " << mMaskPosition
                   << "is of non-boolean type: expected i1, found " << *A->getType() << "\n";
            return false;
        }
    }

    // This is not the "real" simd register width, just what we need
    // for the sext/bc construct to make codegen create a ptest.
    const unsigned simdRegWidth = mVectorizationFactor * 32;

    // Create packet-datatypes.
    mVectorTyFloatSIMD  = VectorType::get(Type::getFloatTy(*mContext), mVectorizationFactor);
    mVectorTyIntSIMD    = VectorType::get(Type::getInt32Ty(*mContext), mVectorizationFactor);
    mVectorTyDoubleSIMD = VectorType::get(Type::getDoubleTy(*mContext), mVectorizationFactor);
    mVectorTyLongSIMD   = VectorType::get(Type::getInt64Ty(*mContext), mVectorizationFactor);
    mVectorTyBoolSIMD   = VectorType::get(Type::getInt1Ty(*mContext), mVectorizationFactor);
    mScalarTyIntSIMD    = Type::getIntNTy(*mContext, simdRegWidth);

    // Generate constants.
    mConstVecSIMDInt32MinusOne = createPacketConstantInt(-1);
    mConstVecSIMDF32One        = createPacketConstantFloat(1.000000e+00f);
    mConstIntSIMDRegWidthZero  = ConstantInt::get(*mContext, APInt(simdRegWidth,  "0", 10));
    mConstInt32Zero            = ConstantInt::get(*mContext, APInt(32,  "0", 10));
    mConstInt32One             = ConstantInt::get(*mContext, APInt(32,  "1", 10));
    mConstInt32Two             = ConstantInt::get(*mContext, APInt(32,  "2", 10));
    mConstInt32Three           = ConstantInt::get(*mContext, APInt(32,  "3", 10));
    mConstBoolTrue             = Constant::getAllOnesValue(Type::getInt1Ty(*mContext));
    mConstBoolFalse            = Constant::getNullValue(Type::getInt1Ty(*mContext));

    // Initialize "dynamic" stuff.

    mAlignmentScalar  = mDataLayout->getABITypeAlignment(Type::getFloatTy(*mContext));
    mAlignmentPtr     = mDataLayout->getABITypeAlignment(
        PointerType::getUnqual(Type::getFloatTy(*mContext)));
    mAlignmentSIMDPtr = mDataLayout->getABITypeAlignment(
        PointerType::getUnqual(mVectorTyFloatSIMD));
    mAlignmentSIMD   = mDataLayout->getABITypeAlignment(mVectorTyFloatSIMD);

    mConstAlignmentSIMD = ConstantInt::get(*mContext, APInt(32, mAlignmentSIMD));

    if(mModule != mScalarFunction->getParent()) {
        errs() << "ERROR: source function has parent that differs from base module\n";
        return false;
    }

    if(mModule != mSimdFunction->getParent()) {
        errs() << "ERROR: target function has parent that differs from base module\n";
        return false;
    }

    if(mContext != &mModule->getContext()) {
        errs() << "WARNING: context differs from module's context";
    }
    return true;
}

// This function should be called *before* run().
bool
WFVInfo::addSIMDMapping(const Function& scalarFunction,
                        const Function& simdFunction,
                        const int       maskPosition,
                        const bool      mayHaveSideEffects)
{
    if (scalarFunction.getParent() != mModule)
	{
        errs() << "ERROR: scalar function has parent that differs from base module!\n";
        return false;
    }
    if (simdFunction.getParent() != mModule)
	{
        errs() << "ERROR: SIMD function has parent that differs from base module!\n";
        return false;
    }

    // Find out which arguments are UNIFORM and which are VARYING.
    SmallVector<bool, 4> uniformArgs;
    uniformArgs.reserve(scalarFunction.getArgumentList().size());

    Function::const_arg_iterator scalarA = scalarFunction.arg_begin();
    Function::const_arg_iterator simdA   = simdFunction.arg_begin();

    for (Function::const_arg_iterator scalarE = scalarFunction.arg_end();
            scalarA != scalarE; ++scalarA, ++simdA)
    {
        Type*      scalarType = scalarA->getType();
        Type*      simdType   = simdA->getType();
        const bool isUniform  = WFV::typesMatch(scalarType, simdType);

        uniformArgs.push_back(isUniform);
    }

    // Store info.
    const bool success = mFunctionInfoMap.add(scalarFunction,
                                              simdFunction,
                                              maskPosition,
                                              mayHaveSideEffects,
                                              uniformArgs);

    if (!success)
	{
        errs() << "ERROR: Insertion of function mapping failed for function: "
                << scalarFunction.getName() << "\n";
        return false;
    }

	return true;
}

// TODO: Refactor the addCommonMappingsXXX functions below.
//       - Make use of the supplied flags
//       - Remove redundancies etc.
//       - Eventually, add only those functions that we need.
bool
WFVInfo::addCommonMappings(const bool useSSE,
                           const bool useSSE41,
                           const bool useSSE42,
                           const bool useAVX,
                           const bool useNEON)
{
    // Read library functions (sin, cos, etc. variants in SSE, AVX, etc.)
    // and create mappings.
    if (useSSE || useSSE41 || useSSE42)
    {
        addCommonMappingsSSE(useSSE41, useSSE42);
    }
    else if (useAVX)
    {
        addCommonMappingsAVX();
    }
    else if (useNEON)
    {
        addCommonMappingsNEON();
    }

    return true;
}


namespace {

bool
checkSanity(const Function& f,
            const bool      isOpUniform,
            const bool      isOpVarying,
            const bool      isOpSequential,
            const bool      isOpSequentialGuarded,
            const bool      isResultUniform,
            const bool      isResultVector,
            const bool      isResultScalars,
            const bool      isAligned,
            const bool      isIndexSame,
            const bool      isIndexConsecutive)
{
    bool isSane = true;

    if (!isOpSequential && !isOpSequentialGuarded && isResultScalars)
    {
        errs() << "ERROR while adding SIMD semantics for function '"
            << f.getName() << "': Only OP_SEQUENTIAL functions can be RESULT_SCALARS!\n";
        isSane = false;
    }

    if (isOpUniform)
    {
        if (!isResultUniform)
        {
            errs() << "ERROR while adding SIMD semantics for function '"
                << f.getName() << "': OP_UNIFORM function has to be RESULT_SAME!\n";
            isSane = false;
        }
        if (!isIndexSame)
        {
            errs() << "ERROR while adding SIMD semantics for function '"
                << f.getName() << "': OP_UNIFORM function has to be INDEX_SAME!\n";
            isSane = false;
        }
    }
    else
    {
        if (isResultUniform)
        {
            errs() << "ERROR while adding SIMD semantics for function '"
                << f.getName() << "': Only OP_UNIFORM function can be RESULT_SAME!\n";
            isSane = false;
        }
        if (isIndexSame)
        {
            errs() << "ERROR while adding SIMD semantics for function '"
                << f.getName() << "': Only OP_UNIFORM function can be INDEX_SAME!\n";
            isSane = false;
        }
    }

    return isSane;
}

bool
checkSanity(const Argument& arg,
            const bool      isResultUniform,
            const bool      isResultVector,
            const bool      isResultScalars,
            const bool      isAligned,
            const bool      isIndexSame,
            const bool      isIndexConsecutive)
{
    bool isSane = true;

    if ((isResultUniform + isResultVector + isResultScalars) != 1)
    {
        errs() << "ERROR while adding SIMD semantics for argument '"
            << arg.getName() << "': argument has to have exactly one RESULT_ property!\n";
        isSane = false;
    }

    if ((isIndexSame + isIndexConsecutive) != 1)
    {
        errs() << "ERROR while adding SIMD semantics for argument '"
            << arg.getName() << "': argument has to have exactly one INDEX_ property!\n";
        isSane = false;
    }

    if (isIndexSame != isResultUniform)
    {
        errs() << "ERROR while adding SIMD semantics for argument '"
            << arg.getName() << "': INDEX_SAME property has to match RESULT_UNIFORM!\n";
        isSane = false;
    }

    return isSane;
}

bool
checkSanity(const Instruction& inst,
            const bool         isOpUniform,
            const bool         isOpVarying,
            const bool         isOpSequential,
            const bool         isOpSequentialGuarded,
            const bool         isResultUniform,
            const bool         isResultVector,
            const bool         isResultScalars,
            const bool         isAligned,
            const bool         isIndexSame,
            const bool         isIndexConsecutive)
{
    bool isSane = true;

    if (!isOpSequential && !isOpSequentialGuarded && isResultScalars)
    {
        errs() << "ERROR while adding SIMD semantics for instruction '"
            << inst << "': Only OP_SEQUENTIAL instructions can be RESULT_SCALARS!\n";
        isSane = false;
    }

    if (isOpUniform)
    {
        if (!isResultUniform)
        {
            errs() << "ERROR while adding SIMD semantics for instruction '"
                << inst << "': OP_UNIFORM instruction has to be RESULT_SAME!\n";
            isSane = false;
        }
        if (!isIndexSame)
        {
            errs() << "ERROR while adding SIMD semantics for instruction '"
                << inst << "': OP_UNIFORM instruction has to be INDEX_SAME!\n";
            isSane = false;
        }
    }
    else
    {
        if (isResultUniform)
        {
            errs() << "ERROR while adding SIMD semantics for instruction '"
                << inst << "': Only OP_UNIFORM instruction can be RESULT_SAME!\n";
            isSane = false;
        }
        if (isIndexSame)
        {
            errs() << "ERROR while adding SIMD semantics for instruction '"
                << inst << "': Only OP_UNIFORM instruction can be INDEX_SAME!\n";
            isSane = false;
        }
    }

    return isSane;
}

}

// This function should be called *before* run()
bool
WFVInfo::addSIMDSemantics(const Function& f,
                          const bool      isOpUniform,
                          const bool      isOpVarying,
                          const bool      isOpSequential,
                          const bool      isOpSequentialGuarded,
                          const bool      isResultUniform,
                          const bool      isResultVector,
                          const bool      isResultScalars,
                          const bool      isAligned,
                          const bool      isIndexSame,
                          const bool      isIndexConsecutive)
{
    bool allSuccessful = true;

    // Check sanity of properties.
    if (!checkSanity(f, isOpUniform, isOpVarying, isOpSequential, isOpSequentialGuarded,
                     isResultUniform, isResultVector, isResultScalars,
                     isAligned, isIndexSame, isIndexConsecutive))
    {
        return false;
    }

    // Add mappings.
    for (Function::const_user_iterator U=f.user_begin(),
        UE=f.user_end(); U!=UE; ++U)
    {
        assert (isa<Instruction>(*U));
        const Instruction* useI = cast<Instruction>(*U);
        assert (useI->getParent() && useI->getParent()->getParent());

        // Ignore uses in other functions.
        if (useI->getParent()->getParent() != mScalarFunction) continue;

        const bool success = mValueInfoMap.add(*useI,
                                               isOpUniform,
                                               isOpVarying,
                                               isOpSequential,
                                               isOpSequentialGuarded,
                                               isResultUniform,
                                               isResultVector,
                                               isResultScalars,
                                               isAligned,
                                               isIndexSame,
                                               isIndexConsecutive);

        if (!success)
        {
            errs() << "ERROR: Insertion of value analysis info failed for use of function: "
                << *useI << "\n";
            allSuccessful = false;
        }
    }

    return allSuccessful;
}

// This function should be called *before* run()
bool
WFVInfo::addSIMDSemantics(const Argument& arg,
                          const bool      isResultUniform,
                          const bool      isResultVector,
                          const bool      isResultScalars,
                          const bool      isAligned,
                          const bool      isIndexSame,
                          const bool      isIndexConsecutive)
{
    if (arg.getParent() != mScalarFunction)
    {
        errs() << "ERROR while adding SIMD semantics for argument '"
            << arg.getName() << "': argument is no argument of to-be vectorized function!\n";
        return false;
    }

    // Check sanity of properties.
    if (!checkSanity(arg, isResultUniform, isResultVector, isResultScalars,
                     isAligned, isIndexSame, isIndexConsecutive))
    {
        return false;
    }

    // Add mappings.
    const bool success = mValueInfoMap.add(arg,
                                           false,
                                           false,
                                           false,
                                           false,
                                           isResultUniform,
                                           isResultVector,
                                           isResultScalars,
                                           isAligned,
                                           isIndexSame,
                                           isIndexConsecutive);

    if (!success)
    {
        errs() << "ERROR: Insertion of value analysis info failed for argument: "
            << arg << "\n";
    }

    return success;
}

// This function should be called *before* run()
bool
WFVInfo::addSIMDSemantics(const Instruction& inst,
                          const bool         isOpUniform,
                          const bool         isOpVarying,
                          const bool         isOpSequential,
                          const bool         isOpSequentialGuarded,
                          const bool         isResultUniform,
                          const bool         isResultVector,
                          const bool         isResultScalars,
                          const bool         isAligned,
                          const bool         isIndexSame,
                          const bool         isIndexConsecutive)
{
    // Check sanity of properties.
    if (!checkSanity(inst, isOpUniform, isOpVarying, isOpSequential, isOpSequentialGuarded,
                     isResultUniform, isResultVector, isResultScalars,
                     isAligned, isIndexSame, isIndexConsecutive))
    {
        return false;
    }

    const bool success = mValueInfoMap.add(inst,
                                           isOpUniform,
                                           isOpVarying,
                                           isOpSequential,
                                           isOpSequentialGuarded,
                                           isResultUniform,
                                           isResultVector,
                                           isResultScalars,
                                           isAligned,
                                           isIndexSame,
                                           isIndexConsecutive);

    if (!success)
    {
        errs() << "ERROR: Insertion of value analysis info failed for instruction: "
            << inst << "\n";
        return false;
    }

    return true;
}


// TODO: The following functions do not exist as scalar intrinsics,
//       so we have to create them manually:
//       * fmodf  -> SIMD version available in IR directly
//       * addsub -> SIMD version available in AVX / SSE3
//       * sincos -> SIMD version available in WFV lib
void
WFVInfo::addCommonMappingsSSE(const bool useSSE41, const bool useSSE42)
{
    // Store mappings of library functions.

    Type*         floatTy  = Type::getFloatTy(*mContext);
    //Type*         doubleTy = Type::getDoubleTy(*mContext);
    FunctionType* fnFloatTy1  = FunctionType::get(floatTy,
                                                  ArrayRef<Type*>(floatTy),
                                                  false);
    //FunctionType* fnDoubleTy1 = FunctionType::get(doubleTy,
                                                  //ArrayRef<Type*>(doubleTy),
                                                  //false);
    std::vector<Type*> types;
    types.push_back(floatTy);
    types.push_back(floatTy);
    FunctionType* fnFloatTy2 = FunctionType::get(floatTy,
                                                 ArrayRef<Type*>(types),
                                                 false);
    //types.clear();
    //types.push_back(doubleTy);
    //types.push_back(doubleTy);
    //FunctionType* fnDoubleTy2 = FunctionType::get(doubleTy,
                                                  //ArrayRef<Type*>(types),
                                                  //false);

#define WFV_GET_OR_CREATE_FUNCTION(fnName, fnType) \
    Function* fnName ## _scalar = mModule->getFunction(#fnName) ? \
        mModule->getFunction(#fnName) : \
        Function::Create(fnType, \
                         GlobalValue::InternalLinkage, \
                         #fnName, \
                         mModule); \
    fnName ## _scalar->setLinkage(GlobalValue::ExternalLinkage); \
    assert (fnName ## _scalar)

#define WFV_GET_INTRINSIC(avxName, sseName) \
    Function* sseName ## _simd = \
        Intrinsic::getDeclaration(mModule, Intrinsic::x86_sse_##sseName); \
    assert (sseName ## _simd);

#define WFV_ADD_MATHFUN_MAPPING(fnName) \
    { \
        Function* scalarFn = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(floatTy)); \
        Function* simdFn   = mModule->getFunction(#fnName "_ps"); \
        assert (scalarFn && simdFn); \
        addSIMDMapping(*scalarFn, *simdFn, -1, false); \
    } ((void)0)

#define WFV_ADD_LLVM_INTERNAL_MAPPING(fnName) \
    { \
        Function* scalarFn = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(floatTy)); \
        Function* simdFn   = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(mVectorTyFloatSIMD)); \
        assert (scalarFn && simdFn); \
        addSIMDMapping(*scalarFn, *simdFn, -1, false); \
    } ((void)0)



    {
        Function* scalarFn = Intrinsic::getDeclaration(mModule,
                                                       Intrinsic::sqrt,
                                                       ArrayRef<Type*>(floatTy));
        assert (scalarFn);
        WFV_GET_INTRINSIC(sqrt_ps_256, sqrt_ps);
        addSIMDMapping(*scalarFn, *sqrt_ps_simd, -1, false);
    }

    {
        WFV_GET_OR_CREATE_FUNCTION(rsqrtf, fnFloatTy1);
        WFV_GET_INTRINSIC(rsqrt_ps_256, rsqrt_ps);
        addSIMDMapping(*rsqrtf_scalar, *rsqrt_ps_simd, -1, false);
    }
    {
        WFV_GET_OR_CREATE_FUNCTION(min, fnFloatTy2);
        WFV_GET_INTRINSIC(min_ps_256, min_ps);
        addSIMDMapping(*min_scalar, *min_ps_simd, -1, false);
    }
    {
        WFV_GET_OR_CREATE_FUNCTION(max, fnFloatTy2);
        WFV_GET_INTRINSIC(max_ps_256, max_ps);
        addSIMDMapping(*max_scalar, *max_ps_simd, -1, false);
    }

    if (useSSE41 || useSSE42)
    {
        WFV_GET_OR_CREATE_FUNCTION(roundf, fnFloatTy1);
        WFV_GET_OR_CREATE_FUNCTION(ceilf,  fnFloatTy1);
        WFV_GET_OR_CREATE_FUNCTION(floorf, fnFloatTy1);

        Function* simdFn = Intrinsic::getDeclaration(mModule, Intrinsic::x86_sse41_round_ps);
        assert (simdFn);
        addSIMDMapping(*roundf_scalar, *simdFn, -1, false);
        addSIMDMapping(*ceilf_scalar,  *simdFn, -1, false);
        addSIMDMapping(*floorf_scalar, *simdFn, -1, false);
    }

    // TODO: Add mappings for double.
    WFV_ADD_MATHFUN_MAPPING(sin);
    WFV_ADD_MATHFUN_MAPPING(cos);
    WFV_ADD_MATHFUN_MAPPING(log);
    WFV_ADD_MATHFUN_MAPPING(log2);
    WFV_ADD_MATHFUN_MAPPING(exp);
    WFV_ADD_MATHFUN_MAPPING(exp2);
    WFV_ADD_MATHFUN_MAPPING(fabs);
#ifdef WFV_USE_NATIVE_POW_PS
    WFV_ADD_MATHFUN_MAPPING(pow);
#endif

    //WFV_ADD_LLVM_INTERNAL_MAPPING(sqrt);
    WFV_ADD_LLVM_INTERNAL_MAPPING(powi);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(sin);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(cos);
    WFV_ADD_LLVM_INTERNAL_MAPPING(pow);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(exp);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(exp2);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(log);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(log2);
    WFV_ADD_LLVM_INTERNAL_MAPPING(log10);
    WFV_ADD_LLVM_INTERNAL_MAPPING(fma);
    // TODO: Enable these for SSE as well (and for double).
    //WFV_ADD_LLVM_INTERNAL_MAPPING(fabs);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(floor);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(ceil);
#ifdef LLVM_33_or_later
    WFV_ADD_LLVM_INTERNAL_MAPPING(trunk);
    WFV_ADD_LLVM_INTERNAL_MAPPING(rint);
    WFV_ADD_LLVM_INTERNAL_MAPPING(nearbyint);
#endif
    WFV_ADD_LLVM_INTERNAL_MAPPING(fmuladd);


#undef WFV_GET_OR_CREATE_FUNCTION
#undef WFV_GET_INTRINSIC
#undef WFV_ADD_MATHFUN_MAPPING
#undef WFV_ADD_LLVM_INTERNAL_MAPPING
}

// TODO: The following functions do not exist as scalar intrinsics,
//       so we have to create them manually:
//       * fmodf  -> SIMD version available in IR directly
//       * addsub -> SIMD version available in AVX / SSE3
//       * sincos -> SIMD version available in WFV lib
void
WFVInfo::addCommonMappingsAVX()
{
    // Store mappings of library functions.

    Type*         floatTy  = Type::getFloatTy(*mContext);
    Type*         doubleTy = Type::getDoubleTy(*mContext);
    FunctionType* fnFloatTy1  = FunctionType::get(floatTy,
                                                  ArrayRef<Type*>(floatTy),
                                                  false);
    FunctionType* fnDoubleTy1 = FunctionType::get(doubleTy,
                                                  ArrayRef<Type*>(doubleTy),
                                                  false);
    std::vector<Type*> types;
    types.push_back(floatTy);
    types.push_back(floatTy);
    FunctionType* fnFloatTy2 = FunctionType::get(floatTy,
                                                 ArrayRef<Type*>(types),
                                                 false);
    types.clear();
    types.push_back(doubleTy);
    types.push_back(doubleTy);
    FunctionType* fnDoubleTy2 = FunctionType::get(doubleTy,
                                                  ArrayRef<Type*>(types),
                                                  false);

#define WFV_GET_OR_CREATE_FUNCTION(fnName, fnType) \
    Function* fnName ## _scalar = mModule->getFunction(#fnName) ? \
        mModule->getFunction(#fnName) : \
        Function::Create(fnType, \
                         GlobalValue::InternalLinkage, \
                         #fnName, \
                         mModule); \
    fnName ## _scalar->setLinkage(GlobalValue::ExternalLinkage); \
    assert (fnName ## _scalar)

#define WFV_GET_INTRINSIC(avxName, sseName) \
    Function* sseName ## _simd = \
        Intrinsic::getDeclaration(mModule, Intrinsic::x86_avx_##avxName); \
    assert (sseName ## _simd);

#define WFV_ADD_MATHFUN_MAPPING(fnName) \
    { \
        Function* scalarFn = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(floatTy)); \
        Function* simdFn   = mModule->getFunction(#fnName "256_ps"); \
        assert (scalarFn && simdFn); \
        addSIMDMapping(*scalarFn, *simdFn, -1, false); \
    } ((void)0)

#define WFV_ADD_LLVM_INTERNAL_MAPPING(fnName) \
    { \
        Function* scalarFn = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(floatTy)); \
        Function* simdFn   = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(mVectorTyFloatSIMD)); \
        assert (scalarFn && simdFn); \
        addSIMDMapping(*scalarFn, *simdFn, -1, false); \
    } \
    { \
        Function* scalarFn = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(doubleTy)); \
        Function* simdFn   = Intrinsic::getDeclaration(mModule, \
                                                       Intrinsic::fnName, \
                                                       ArrayRef<Type*>(mVectorTyDoubleSIMD)); \
        assert (scalarFn && simdFn); \
        addSIMDMapping(*scalarFn, *simdFn, -1, false); \
    } ((void)0)



    {
        Function* scalarFn = Intrinsic::getDeclaration(mModule,
                                                       Intrinsic::sqrt,
                                                       ArrayRef<Type*>(floatTy));
        assert (scalarFn);
        WFV_GET_INTRINSIC(sqrt_ps_256, sqrt_ps);
        addSIMDMapping(*scalarFn, *sqrt_ps_simd, -1, false);
    }
    {
        Function* scalarFn = Intrinsic::getDeclaration(mModule,
                                                       Intrinsic::sqrt,
                                                       ArrayRef<Type*>(doubleTy));
        assert (scalarFn);
        WFV_GET_INTRINSIC(sqrt_pd_256, sqrt_pd);
        addSIMDMapping(*scalarFn, *sqrt_pd_simd, -1, false);
    }

    {
        // rsqrt only exists for <8 x float>, not <4 x double>, apparently.
        WFV_GET_OR_CREATE_FUNCTION(rsqrtf, fnFloatTy1);
        WFV_GET_INTRINSIC(rsqrt_ps_256, rsqrt_ps);
        addSIMDMapping(*rsqrtf_scalar, *rsqrt_ps_simd, -1, false);
    }
    {
        WFV_GET_OR_CREATE_FUNCTION(minf, fnFloatTy2);
        WFV_GET_INTRINSIC(min_ps_256, min_ps);
        addSIMDMapping(*minf_scalar, *min_ps_simd, -1, false);
    }
    {
        WFV_GET_OR_CREATE_FUNCTION(min, fnDoubleTy2);
        WFV_GET_INTRINSIC(min_pd_256, min_pd);
        addSIMDMapping(*min_scalar, *min_pd_simd, -1, false);
    }
    {
        WFV_GET_OR_CREATE_FUNCTION(maxf, fnFloatTy2);
        WFV_GET_INTRINSIC(max_ps_256, max_ps);
        addSIMDMapping(*maxf_scalar, *max_ps_simd, -1, false);
    }
    {
        WFV_GET_OR_CREATE_FUNCTION(max, fnDoubleTy2);
        WFV_GET_INTRINSIC(max_pd_256, max_pd);
        addSIMDMapping(*max_scalar, *max_pd_simd, -1, false);
    }

    {
        WFV_GET_OR_CREATE_FUNCTION(roundf, fnFloatTy1);
        WFV_GET_OR_CREATE_FUNCTION(ceilf,  fnFloatTy1);
        WFV_GET_OR_CREATE_FUNCTION(floorf, fnFloatTy1);

        Function* simdFn = Intrinsic::getDeclaration(mModule, Intrinsic::x86_avx_round_ps_256);
        assert (simdFn);
        addSIMDMapping(*roundf_scalar, *simdFn, -1, false);
        addSIMDMapping(*ceilf_scalar,  *simdFn, -1, false);
        addSIMDMapping(*floorf_scalar, *simdFn, -1, false);
    }
    {
        WFV_GET_OR_CREATE_FUNCTION(round, fnDoubleTy1);
        WFV_GET_OR_CREATE_FUNCTION(ceil,  fnDoubleTy1);
        WFV_GET_OR_CREATE_FUNCTION(floor, fnDoubleTy1);

        Function* simdFn = Intrinsic::getDeclaration(mModule, Intrinsic::x86_avx_round_pd_256);
        assert (simdFn);
        addSIMDMapping(*round_scalar, *simdFn, -1, false);
        addSIMDMapping(*ceil_scalar,  *simdFn, -1, false);
        addSIMDMapping(*floor_scalar, *simdFn, -1, false);
    }


    // TODO: Add mappings for double.
    WFV_ADD_MATHFUN_MAPPING(sin);
    WFV_ADD_MATHFUN_MAPPING(cos);
    WFV_ADD_MATHFUN_MAPPING(log);
    WFV_ADD_MATHFUN_MAPPING(log2);
    WFV_ADD_MATHFUN_MAPPING(exp);
    WFV_ADD_MATHFUN_MAPPING(exp2);
    WFV_ADD_MATHFUN_MAPPING(fabs);
#ifdef WFV_USE_NATIVE_POW_PS
    WFV_ADD_MATHFUN_MAPPING(pow);
#endif

    //WFV_ADD_LLVM_INTERNAL_MAPPING(sqrt);
    WFV_ADD_LLVM_INTERNAL_MAPPING(powi);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(sin);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(cos);
    WFV_ADD_LLVM_INTERNAL_MAPPING(pow);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(exp);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(exp2);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(log);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(log2);
    WFV_ADD_LLVM_INTERNAL_MAPPING(log10);
    WFV_ADD_LLVM_INTERNAL_MAPPING(fma);
    //WFV_ADD_LLVM_INTERNAL_MAPPING(fabs);
    WFV_ADD_LLVM_INTERNAL_MAPPING(trunc);
    WFV_ADD_LLVM_INTERNAL_MAPPING(floor);
    WFV_ADD_LLVM_INTERNAL_MAPPING(ceil);
    WFV_ADD_LLVM_INTERNAL_MAPPING(rint);
    WFV_ADD_LLVM_INTERNAL_MAPPING(nearbyint);
    WFV_ADD_LLVM_INTERNAL_MAPPING(fmuladd);

#undef WFV_GET_OR_CREATE_FUNCTION
#undef WFV_GET_INTRINSIC
#undef WFV_ADD_MATHFUN_MAPPING
#undef WFV_ADD_LLVM_INTERNAL_MAPPING
}

void
WFVInfo::addCommonMappingsNEON()
{
    assert (false && "not implemented!");
}

// NOTE: we must not pre-generate this due to possibly different address spaces
const PointerType*
WFVInfo::getPointerVectorType(const PointerType* oldType) const
{
    return PointerType::get(VectorType::get(oldType->getElementType(), mVectorizationFactor),
                            oldType->getAddressSpace());
}

Constant*
WFVInfo::createPacketConstantInt(const int c) const
{
    std::vector<Constant*> cVec; //packet of 'vectorizationFactor' int32
    ConstantInt* const_int32 = ConstantInt::get(*mContext, APInt(32, c));
    for (unsigned i=0; i<mVectorizationFactor; ++i)
    {
        cVec.push_back(const_int32);
    }
    return ConstantVector::get(ArrayRef<Constant*>(cVec));
}

Constant*
WFVInfo::createPacketConstantFloat(const float c) const
{
    std::vector<Constant*> fVec; //packet of 'vectorizationFactor' f32
    ConstantFP* const_f32 = ConstantFP::get(*mContext, APFloat(c));
    for (unsigned i=0; i<mVectorizationFactor; ++i)
    {
        fVec.push_back(const_f32);
    }
    return ConstantVector::get(ArrayRef<Constant*>(fVec));
}
