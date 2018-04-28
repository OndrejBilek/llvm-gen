#ifndef OPT_TAIL_RECURSION_H
#define OPT_TAIL_RECURSION_H

#include "mila.h"
#include "llvm.h"

namespace mila {

namespace tailrecursion {

class Optimization : public llvm::FunctionPass {
 public:
  static char ID;

  Optimization():
      llvm::FunctionPass(ID) {
  }

  llvm::StringRef getPassName() const override {
    return "TailRecursion";
  }

  bool runOnFunction(llvm::Function & f) override {
    return false;
  }

};

}
}
#endif
