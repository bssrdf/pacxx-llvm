/* Copyright (C) University of Muenster - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Michael Haidl <michael.haidl@uni-muenster.de>, 2010-2014
*/

#include <cassert>
#include <iostream>
#include <vector>

#define DEBUG_TYPE "nvvm-transformation"

#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/PACXXTransforms.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "CallVisitor.h"
#include "ModuleHelper.h"

using namespace llvm;
using namespace std;
using namespace pacxx;

namespace {

struct NVVMPass : public ModulePass {
  static char ID;
  NVVMPass() : ModulePass(ID) {}
  virtual ~NVVMPass() {}

  virtual bool runOnModule(Module &M) {
    _M = &M;
    bool modified = true;
    vector<Function *> deleted_functions;

    unsigned ptrSize = M.getDataLayout().getPointerSizeInBits();

    if (ptrSize == 64){
      M.setTargetTriple("nvptx64-unknown-unknown");

      M.setDataLayout("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:"
                      "64-f32:32:32-f64:64:64-v16:16:16-v32:32:32-v64:64:64-"
                      "v128:128:128-n16:32:64");
    }
    else {
      M.setTargetTriple("nvptx-unknown-unknown");

      M.setDataLayout("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:"
                          "64-f32:32:32-f64:64:64-v16:16:16-v32:32:32-v64:64:64-"
                          "v128:128:128-n16:32:64");
    }

    auto kernels = pacxx::getTagedFunctions(&M, "nvvm.annotations", "kernel");

    auto visitor = make_CallVisitor([](CallInst *I) {
      if (I->isInlineAsm()) {
        if (auto ASM = dyn_cast<InlineAsm>(I->getCalledValue())) {
          if (ASM->getConstraintString().find("memory") != string::npos) {
            I->eraseFromParent();
            return;
          }
        }
      }
    });

    for (auto &F : kernels)
      visitor.visit(F);

    auto called_functions = visitor.get();

    cleanupDeadCode(&M);

    for (auto &F : kernels) {
      F->setCallingConv(CallingConv::PTX_Kernel);
      F->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
      F->setVisibility(GlobalValue::VisibilityTypes::DefaultVisibility);
    }

    return modified;
  }

private:
  Module *_M;
};

char NVVMPass::ID = 0;
static RegisterPass<NVVMPass> X("nvvm", "LLVM to NVVM IR pass", false, false);
}

namespace llvm {
Pass *createPACXXNvvmPass() { return new NVVMPass(); }
}
