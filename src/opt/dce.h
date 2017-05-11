#ifndef OPT_DEAD_CODE_H
#define OPT_DEAD_CODE_H

#include "mila.h"
#include "llvm.h"

namespace mila {

namespace dce {

class Optimization : public llvm::FunctionPass {
 public:
  static char ID;

  Optimization():
      llvm::FunctionPass(ID) {
  }

  char const * getPassName() const override {
    return "DeadCodeElimination";
  }

  bool runOnFunction(llvm::Function & f) override {
    return false;
  }

};

}
}
#endif
