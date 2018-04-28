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

  llvm::StringRef getPassName() const override {
    return "DeadInstructionRemovalOptimization";
  }

  bool runOnFunction(llvm::Function & f) override {
    changed_ = false;

    for (llvm::inst_iterator instIter = llvm::inst_begin(f), e = llvm::inst_end(f); instIter != e; ++instIter) {
      llvm::Instruction *i = &*instIter;
      if (i != nullptr && llvm::isInstructionTriviallyDead(i)) {
        i->eraseFromParent();
        changed_ = true;
      }
    }

    return changed_;
  }

 private:
  bool changed_;

};

}
}
