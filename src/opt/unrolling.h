#ifndef OPT_LOOP_UNROLLING_H
#define OPT_LOOP_UNROLLING_H

#include "mila.h"
#include "llvm.h"

namespace mila {

namespace unrolling {

class Optimization : public llvm::FunctionPass {
 public:
  static char ID;

  Optimization():
      llvm::FunctionPass(ID) {
  }

  char const * getPassName() const override {
    return "LoopUnrolling";
  }

  bool runOnFunction(llvm::Function & f) override {
    return false;
  }

};

}
}
#endif
