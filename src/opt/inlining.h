#ifndef OPT_INLINING_H
#define OPT_INLINING_H

#include "mila.h"
#include "llvm.h"

namespace mila {

namespace inlining {

class Optimization : public llvm::FunctionPass {
 public:
  static char ID;

  Optimization():
      llvm::FunctionPass(ID) {
  }

  llvm::StringRef getPassName() const override {
    return "Inlining";
  }

  bool runOnFunction(llvm::Function & f) override {
    return false;
  }

};

}
}
#endif
