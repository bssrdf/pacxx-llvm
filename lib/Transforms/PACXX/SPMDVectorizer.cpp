// Created by lars

#include <stdlib.h>
#include "Log.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/ValueTracking.h"
#include "rv/rv.h"
#include "rv/analysis/maskAnalysis.h"
#include "rv/transform/loopExitCanonicalizer.h"
#include "ModuleHelper.h"

using namespace llvm;
using namespace std;
using namespace pacxx;


namespace {

    class SPMDVectorizer : public llvm::ModulePass {

    public:
        static char ID;

        SPMDVectorizer() : llvm::ModulePass(ID) { initializeSPMDVectorizerPass(*PassRegistry::getPassRegistry()); }

        virtual ~SPMDVectorizer() {}

        void releaseMemory() override;

        void getAnalysisUsage(AnalysisUsage& AU) const override;

        bool runOnModule(llvm::Module& M) override;

    private:

        Function *createScalarCopy(Module *M, Function *kernel);

        Function *createVectorizedKernelHeader(Module *M, Function *kernel);

        unsigned determineVectorWidth(Function *F, rv::VectorizationInfo &vecInfo, TargetTransformInfo *TTI);

        void prepareForVectorization(Function *kernel, rv::VectorizationInfo &vecInfo);

        bool modifyWrapperLoop(Function *dummyFunction, Function *vectorizedKernel, Function *kernel,
                               unsigned vectorWidth, Module& M);

        Function *createKernelSpecificFoo(Module &M, Function *F, Function *kernel);

        bool modifyOldLoop(Function *F);

        BasicBlock *determineOldLoopPreHeader(Function *F);

        Value *determineMaxx(Function *F);

        Value *determine_x(Function *F);

    };
}

void SPMDVectorizer::releaseMemory() {}

void SPMDVectorizer::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
}

bool SPMDVectorizer::runOnModule(Module& M) {

    auto &DL = M.getDataLayout();

    bool kernelsVectorized = true;

    auto kernels = getTagedFunctions(&M, "nvvm.annotations", "kernel");

    Function* dummyFunction = M.getFunction("__dummy_kernel");

    for (auto kernel : kernels) {

        bool vectorized = false;

        TargetTransformInfo* TTI = &getAnalysis<TargetTransformInfoWrapperPass>().getTTI(*kernel);

        TargetLibraryInfo *TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

        Function *scalarCopy = createScalarCopy(&M, kernel);

        Function *vectorizedKernel = createVectorizedKernelHeader(&M, scalarCopy);

        rv::PlatformInfo platformInfo(M, TTI, TLI);

        rv::VectorizerInterface vectorizer(platformInfo);

        // build Analysis that is independent of vecInfo
        DominatorTree domTree(*scalarCopy);
        PostDominatorTree postDomTree;
        postDomTree.recalculate(*scalarCopy);
        LoopInfo loopInfo(domTree);

        // Dominance Frontier Graph
        DFG dfg(domTree);
        dfg.create(*scalarCopy);

        // Control Dependence Graph
        CDG cdg(postDomTree);
        cdg.create(*scalarCopy);

        // normalize loop exits
        LoopExitCanonicalizer canonicalizer(loopInfo);
        canonicalizer.canonicalize(*scalarCopy);

        //return and arguments are uniform
        rv::VectorShape resShape = rv::VectorShape::uni(1u);
        rv::VectorShapeVec argShapes;

        for (auto& it : scalarCopy->args()) {
            if(it.getType()->isPointerTy())
                argShapes.push_back(rv::VectorShape::uni(it.getPointerAlignment(DL)));
            else
                argShapes.push_back(rv::VectorShape::uni(DL.getPrefTypeAlignment(it.getType())));
        }

        // tmp mapping to determine possible vector width
        rv::VectorMapping tmpMapping(scalarCopy, vectorizedKernel, 4, -1, resShape, argShapes);

        rv::VectorizationInfo tmpInfo(tmpMapping);

        prepareForVectorization(scalarCopy, tmpInfo);

        // vectorizationAnalysis
        vectorizer.analyze(tmpInfo, cdg, dfg, loopInfo, postDomTree, domTree);

        unsigned vectorWidth = determineVectorWidth(scalarCopy, tmpInfo, TTI);

        rv::VectorMapping targetMapping(scalarCopy, vectorizedKernel, vectorWidth, -1, resShape, argShapes);

        rv::VectorizationInfo vecInfo(targetMapping);

        prepareForVectorization(scalarCopy, vecInfo);

        vectorizer.analyze(vecInfo, cdg, dfg, loopInfo, postDomTree, domTree);

        __verbose("VectorWidth: ", vectorWidth);

        // mask analysis
        auto* maskAnalysis = vectorizer.analyzeMasks(vecInfo, loopInfo);
        assert(maskAnalysis);

        // mask generator
        bool genMaskOk = vectorizer.generateMasks(vecInfo, *maskAnalysis, loopInfo);
        if (!genMaskOk)
            errs() << "mask generation failed.";

        // control conversion
        bool linearizeOk = vectorizer.linearizeCFG(vecInfo, *maskAnalysis, loopInfo, domTree);
        if (!linearizeOk)
            errs() << "linearization failed";

        // Control conversion does not preserve the domTree so we have to rebuild it for now
        const DominatorTree domTreeNew(*vecInfo.getMapping().scalarFn);
        bool vectorizeOk = vectorizer.vectorize(vecInfo, domTreeNew, loopInfo);
        if (!vectorizeOk)
            errs() << "vector code generation failed";

        std::error_code EC;
        raw_fd_ostream OS("vectorized.kernel.ll", EC, sys::fs::F_None);
        vectorizedKernel->print(OS, nullptr);

        // cleanup
        vectorizer.finalize();

        delete maskAnalysis;
        scalarCopy->eraseFromParent();

        if(genMaskOk && linearizeOk && vectorizeOk)
            vectorized = modifyWrapperLoop(dummyFunction, kernel, vectorizedKernel, vectorWidth, M);

        __verbose("vectorized: ", vectorized);

        if (vectorized) {
            vectorizedKernel->setName("__vectorized__" + kernel->getName());
            vectorizedKernel->addFnAttr("simd-size", to_string(vectorWidth));
            kernel->addFnAttr("vectorized");
        } else
            vectorizedKernel->eraseFromParent();

        kernelsVectorized &= vectorized;
    }

    return kernelsVectorized;
}

Function *SPMDVectorizer::createScalarCopy(Module *M, Function *kernel) {

    ValueToValueMapTy valueMap;
    Function* scalarCopy = CloneFunction(kernel, valueMap, nullptr);

    assert (scalarCopy);
    scalarCopy->setCallingConv(kernel->getCallingConv());
    scalarCopy->setAttributes(kernel->getAttributes());
    scalarCopy->setAlignment(kernel->getAlignment());
    scalarCopy->setLinkage(GlobalValue::InternalLinkage);
    scalarCopy->setName(kernel->getName() + ".vectorizer.tmp");

    return scalarCopy;
}

Function *SPMDVectorizer::createVectorizedKernelHeader(Module *M, Function *kernel) {

    Function* vectorizedKernel = Function::Create(kernel->getFunctionType(), GlobalValue::LinkageTypes::ExternalLinkage,
                                           std::string("__vectorized__") + kernel->getName().str(), M);

    Function::arg_iterator AI = vectorizedKernel->arg_begin();
    for(const Argument& I : kernel->args()) {
        AI->setName(I.getName());
    }

    return vectorizedKernel;
}

unsigned SPMDVectorizer::determineVectorWidth(Function *F, rv::VectorizationInfo &vecInfo, TargetTransformInfo *TTI) {

    unsigned MaxWidth = 8;
    const DataLayout &DL = F->getParent()->getDataLayout();

    for (auto &B : *F) {
        for (auto &I : B) {

            if(vecInfo.getVectorShape(I).isVarying() || vecInfo.getVectorShape(I).isContiguous()) {

                bool ignore = false;

                // check if the cast is only used as idx in GEP
                if(isa<CastInst>(&I)) {
                    ignore = true;
                    for (auto user : I.users()) {
                        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(user)) {
                            // the result of the cast is not used as an idx
                            if (std::find(GEP->idx_begin(), GEP->idx_end(), &I) == GEP->idx_end())
                                ignore = false;
                        }
                        else
                            // we have a user different than a GEP
                            ignore = false;
                    }
                }

                if(!ignore) {


                    Type *T = I.getType();

                    if (T->isPointerTy())
                        T = T->getPointerElementType();

                    if (T->isSized())
                        MaxWidth = std::max(MaxWidth, (unsigned) DL.getTypeSizeInBits(T->getScalarType()));
                }
            }
        }
    }

    return TTI->getRegisterBitWidth(true) / MaxWidth;
}

void SPMDVectorizer::prepareForVectorization(Function *kernel, rv::VectorizationInfo &vecInfo) {

    Module *M = kernel->getParent();

    auto &DL = M->getDataLayout();

    for(auto &global : M->globals()) {
        for (User *user: global.users()) {
            if (Instruction *Inst = dyn_cast<Instruction>(user)) {
                if (Inst->getParent()->getParent() == kernel) {
                    vecInfo.setVectorShape(global, rv::VectorShape::uni());
                    break;
                }
            }
        }
    }

    for (llvm::inst_iterator II=inst_begin(kernel), IE=inst_end(kernel); II!=IE; ++II) {
        Instruction *inst = &*II;

        // mark intrinsics
        if (auto CI = dyn_cast<CallInst>(inst)) {
            auto called = CI->getCalledFunction();
            if (called && called->isIntrinsic()) {
                auto intrin_id = called->getIntrinsicID();

                switch (intrin_id) {
                    case Intrinsic::pacxx_read_tid_x: {
                        vecInfo.setVectorShape(*CI, rv::VectorShape::cont(DL.getPrefTypeAlignment(CI->getType())));
                        break;
                    }
                    case Intrinsic::pacxx_read_tid_y:
                    case Intrinsic::pacxx_read_tid_z:
                    case Intrinsic::pacxx_read_ctaid_x:
                    case Intrinsic::pacxx_read_ctaid_y:
                    case Intrinsic::pacxx_read_ctaid_z:
                    case Intrinsic::pacxx_read_nctaid_x:
                    case Intrinsic::pacxx_read_nctaid_y:
                    case Intrinsic::pacxx_read_nctaid_z:
                    case Intrinsic::pacxx_read_ntid_x:
                    case Intrinsic::pacxx_read_ntid_y:
                    case Intrinsic::pacxx_read_ntid_z: {
                        vecInfo.setVectorShape(*CI, rv::VectorShape::uni(DL.getPrefTypeAlignment(CI->getType())));
                        break;
                    }
                    default: break;
                }
            }
        }
    }
    if(Function *barrierFunc = M->getFunction("llvm.pacxx.barrier0"))
        vecInfo.setVectorShape(*barrierFunc, rv::VectorShape::uni(DL.getPrefTypeAlignment(barrierFunc->getType())));
}


bool SPMDVectorizer::modifyWrapperLoop(Function *dummyFunction, Function *kernel, Function *vectorizedKernel,
                                       unsigned vectorWidth, Module& M) {

    auto& ctx = M.getContext();
    auto int32_type = Type::getInt32Ty(ctx);

    Function* F = M.getFunction("__pacxx_block");

    // creating a new pacxx block wrapper, because every kernel could have a different vector width
    Function *vecFoo = createKernelSpecificFoo(M, F, kernel);


    BasicBlock* oldLoopHeader = determineOldLoopPreHeader(vecFoo);
    if(!oldLoopHeader)
        return false;

    Value* maxx = determineMaxx(vecFoo);
    if(!maxx)
        return false;

    Value* __x = determine_x(vecFoo);
    if(!__x)
        return false;

    modifyOldLoop(vecFoo);


    // construct required BasicBlocks
    BasicBlock* loopEnd = BasicBlock::Create(ctx, "loop-end", vecFoo, oldLoopHeader);
    BasicBlock* loopBody = BasicBlock::Create(ctx, "loop-body", vecFoo, loopEnd);
    BasicBlock* loopHeader = BasicBlock::Create(ctx, "loop-header", vecFoo, loopBody);
    BasicBlock* loopPreHeader = BasicBlock::Create(ctx, "pre-header", vecFoo, loopHeader);

    //modify predecessor of oldLoopPreHeader to branch into the newLoopPreHeader
    BasicBlock* predecessor = oldLoopHeader->getUniquePredecessor();
    if(BranchInst* BI = dyn_cast<BranchInst>(predecessor->getTerminator()))
        for(unsigned i = 0; i < BI->getNumSuccessors(); ++i) {
            if(BI->getSuccessor(i) == oldLoopHeader)
                BI->setSuccessor(i, loopPreHeader);
        }

    //insert instructions into loop preHeader
    new StoreInst(ConstantInt::get(int32_type, 0), __x, loopPreHeader);
    BranchInst::Create(loopHeader, loopPreHeader);

    //insert Instruction into loop header
    LoadInst* headerLoadVar = new LoadInst(__x, "loadVar", loopHeader);
    LoadInst* loadMaxx = new LoadInst(maxx, "loadmaxx", loopHeader);
    Instruction* inc = BinaryOperator::CreateAdd(headerLoadVar, ConstantInt::get(int32_type, vectorWidth),
                                                 "increment loop var", loopHeader);
    ICmpInst* varLessThanMaxx = new ICmpInst(*loopHeader, ICmpInst::ICMP_SLT, inc, loadMaxx, "cmp");
    BranchInst::Create(loopBody, oldLoopHeader, varLessThanMaxx, loopHeader);


    InlineFunctionInfo IFI;
    std::vector<Value *> args;

    for (auto U : dummyFunction->users()) {
        if (CallInst* CI = dyn_cast<CallInst>(U)) {

            if(!(CI->getParent()->getParent() == vecFoo)) continue;

            auto argIt = vecFoo->arg_end();
            unsigned numArgs = kernel->arg_size();
            for(unsigned i = 0; i < numArgs; ++i)
                --argIt;

            for(unsigned i = 0; i < numArgs; ++i) {
                args.push_back(&*argIt);
                argIt++;
            }
        }
    }

    CallInst* vecFunction = CallInst::Create(vectorizedKernel, args, "", loopBody);


    BranchInst::Create(loopEnd, loopBody);

    //insert instructions into loop end
    LoadInst* loadLoopVar = new LoadInst(__x, "loadVar", loopEnd);
    Instruction* incLoopEnd = BinaryOperator::CreateAdd(loadLoopVar, ConstantInt::get(int32_type, vectorWidth),
                                                        "increment loop var", loopEnd);
    new StoreInst(incLoopEnd, __x, loopEnd);
    BranchInst::Create(loopHeader, loopEnd);

    InlineFunction(vecFunction, IFI);

    for (auto U : dummyFunction->users()) {
        if (CallInst* CI = dyn_cast<CallInst>(U)) {

            if(!(CI->getParent()->getParent() == vecFoo)) continue;

            CallInst *kernelCall = CallInst::Create(kernel, args, "", CI);
            InlineFunction(kernelCall, IFI);
            CI->eraseFromParent();
        }
    }

    return true;
}

Function *SPMDVectorizer::createKernelSpecificFoo(Module &M, Function *F, Function *kernel) {

    ValueToValueMapTy VMap;

    SmallVector<Type *, 8> Params;

    for(auto &arg : F->args()) {
        Params.push_back(arg.getType());
    }

    for(auto &arg : kernel->args()) {
        Params.push_back(arg.getType());
    }

    FunctionType *FTy = FunctionType::get(Type::getVoidTy(M.getContext()), Params, false);

    Function *vecFoo = cast<Function>(
            M.getOrInsertFunction("__vectorized__pacxx_block__" + kernel->getName().str(), FTy));

    SmallVector<ReturnInst *, 3> rets;
    for (auto &BB : kernel->getBasicBlockList())
        for (auto &I : BB.getInstList()) {
            if (auto r = dyn_cast<ReturnInst>(&I)) {
                rets.push_back(r);
            }
        }

    auto DestI = vecFoo->arg_begin();
    for (auto I = F->arg_begin(); I != F->arg_end(); ++I) {
        DestI->setName(I->getName().str());
        VMap[cast<Value>(I)] = cast<Value>(DestI++);
    }

    CloneFunctionInto(vecFoo, F, VMap, false, rets);

    vecFoo->setCallingConv(F->getCallingConv());
    vecFoo->setAttributes(F->getAttributes());
    vecFoo->setAlignment(F->getAlignment());
    vecFoo->setLinkage(F->getLinkage());

    return vecFoo;
}


bool SPMDVectorizer::modifyOldLoop(Function *F) {

    std::vector<StoreInst *> storesToRemove;

    BasicBlock* oldLoopHeader = determineOldLoopPreHeader(F);
    if(!oldLoopHeader)
        return false;

    for (auto &I : *(oldLoopHeader)) {
        if(StoreInst* SI = dyn_cast<StoreInst>(&I)) {
            if(SI->getPointerOperand()->getName() == "__x") {
                storesToRemove.push_back(SI);
            }
        }
    }

    for(auto SI : storesToRemove) {
        SI->eraseFromParent();
    }

    return true;
}

BasicBlock * SPMDVectorizer::determineOldLoopPreHeader(Function *F) {
    LoopInfo* LI = &getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
    // we know that there are only 3 nested loops
    Loop* xLoop = ((*LI->begin())->getSubLoops().front())->getSubLoops().front();
    BasicBlock* loopPreHeader = xLoop->getLoopPredecessor();
    return loopPreHeader;
}

Value * SPMDVectorizer::determineMaxx(Function *F) {
    Value* maxx;
    Function::arg_iterator argIt = F->arg_begin();
    if(!((&*argIt)->getName() == "__maxx"))
        ++argIt;
    for (auto U : (&*argIt)->users()) {
        if (StoreInst* SI = dyn_cast<StoreInst>(U)) {
            maxx = SI->getPointerOperand();
        }
    }
    return maxx;
}

Value *SPMDVectorizer::determine_x(Function *F) {
    Value* __x;
    for (auto &B : *F) {
        for (auto &I : B) {
            if (AllocaInst * alloca = dyn_cast<AllocaInst>(&I))
                if(alloca->getMetadata("pacxx_read_tid_x") != nullptr)
                    __x = alloca;
        }
    }
    return __x;
}

char SPMDVectorizer::ID = 0;

INITIALIZE_PASS_BEGIN(SPMDVectorizer, "spmd",
                "SPMD vectorizer", true, true)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(SPMDVectorizer, "spmd",
                "SPMD vectorizer", true, true)

namespace llvm {
    llvm::Pass *createSPMDVectorizerPass() {
        return new SPMDVectorizer();
    }
}
