#pragma once

#include "mila.h"
#include "llvm.h"

namespace mila {

namespace die {

class Optimization : public llvm::FunctionPass {
 public:
  static char ID;

  Optimization():
      llvm::FunctionPass(ID) {
  }

  char const * getPassName() const override {
    return "DeadInstructionRemovalOptimization";
  }

  bool runOnFunction(llvm::Function & f) override {
    changed_ = false;
    for (llvm::BasicBlock & b : f) {
      auto i = b.begin();
      while (i != b.end()) {
        if (i->getNumUses() == 0 and not i->isTerminator() and not llvm::dyn_cast<llvm::StoreInst>(i) and not llvm::dyn_cast<llvm::CallInst>(i)) {
          llvm::Instruction * old = i;
          ++i;
          old->eraseFromParent();
          changed_ = true;

        } else {
          ++i;
        }
      }
    }
    return changed_;
  }

 private:
  bool changed_;

};

}
}
