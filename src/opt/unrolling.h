#ifndef OPT_LOOP_UNROLLING_H
#define OPT_LOOP_UNROLLING_H

#include "mila.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm.h"
#include <iostream>

namespace mila {

namespace unrolling {

class Optimization : public llvm::LoopPass {
 public:
  static char ID;

  Optimization():
      llvm::LoopPass(ID) {
  }

  llvm::StringRef getPassName() const override {
    return "LoopUnrolling";
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.addRequired<llvm::LoopInfoWrapperPass>();
    AU.addRequired<llvm::ScalarEvolutionWrapperPass>();
    AU.addRequired<llvm::DominatorTreeWrapperPass>();
    AU.addRequired<llvm::AssumptionCacheTracker>();
  }

  bool runOnLoop(llvm::Loop *L, llvm::LPPassManager &LPM) override {
    bool modified = false;
    llvm::Function &F = *L->getHeader()->getParent();

    auto &LI = getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();
    auto &SE = getAnalysis<llvm::ScalarEvolutionWrapperPass>().getSE();
    auto &DT = getAnalysis<llvm::DominatorTreeWrapperPass>().getDomTree();
    auto &AC = getAnalysis<llvm::AssumptionCacheTracker>().getAssumptionCache(F);
    llvm::OptimizationRemarkEmitter ORE(&F);

    for(auto i : LI) {
      auto loop = &*i;
      auto tripCount = SE.getSmallConstantTripCount(loop);
      auto tripMultiple = SE.getSmallConstantTripMultiple(loop);
      auto result = llvm::UnrollLoop(
              loop, 2,
              tripCount,
              false, true, true, false, false,
              tripMultiple, 0, true,
              &LI, &SE, &DT, &AC, &ORE,
              true);
      if (result != llvm::LoopUnrollResult::Unmodified) {
        modified = true;
      }
    }

    if (modified) {
      std::cout << "Works!" << std::endl;
    } else {
      std::cout << "Nope" << std::endl;
    }

    return modified;
  }

};

}
}
#endif
