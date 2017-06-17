#ifndef OPT_DEAD_CODE_H
#define OPT_DEAD_CODE_H

#include "mila.h"
#include "llvm.h"

namespace mila {

namespace dce {

class Optimization : public llvm::FunctionPass {
 public:
  static char ID;

  Optimization() :
      llvm::FunctionPass(ID) {
  }

  char const *getPassName() const override {
    return "DeadCodeElimination";
  }

  bool runOnFunction(llvm::Function &f) override {
    llvm::SmallPtrSet<llvm::Instruction *, 128> alive;
    llvm::SmallVector<llvm::Instruction *, 128> list;

    // Collect root instr that are live
    for (llvm::Instruction &instr : llvm::inst_range(f)) {
      if (instr.isTerminator() || instr.mayHaveSideEffects()) {
        alive.insert(&instr);
        list.push_back(&instr);
      }
    }

    // Propagate to operands
    while (!list.empty()) {
      llvm::Instruction *currInstr = list.pop_back_val();
      for (llvm::Use &ops : currInstr->operands()) {
        if (llvm::Instruction *inst = llvm::dyn_cast<llvm::Instruction>(ops)) {
          if (alive.insert(inst).second) {
            list.push_back(inst);
          }
        }
      }
    }

    // Dead if not proven otherwise
    for (llvm::Instruction &i : llvm::inst_range(f)) {
      if (!alive.count(&i)) {
        list.push_back(&i);
        i.dropAllReferences();
      }
    }

    for (llvm::Instruction *&i: list) {
      i->eraseFromParent();
    }

    return !list.empty();
  }

};

}
}
#endif
