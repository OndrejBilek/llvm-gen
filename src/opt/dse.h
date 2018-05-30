#ifndef OPT_DSE_H
#define OPT_DSE_H

#include "mila.h"
#include "llvm.h"
#include <iostream>

namespace mila {

    namespace dse {

        class Optimization : public llvm::FunctionPass {
        public:
            static char ID;

            Optimization() :
                    llvm::FunctionPass(ID) {
            }

            llvm::StringRef getPassName() const override {
                return "DeadStoreElimination";
            }


            bool runOnFunction(llvm::Function &f) override {
                llvm::SmallVector<llvm::Instruction *, 32> toRemove;

                for (llvm::BasicBlock &bb : f) {
                    for (llvm::Instruction &instr : bb) {
                        if (llvm::isa<llvm::AllocaInst>(instr)) {
                            bool hasLoads = false;
                            llvm::SmallVector<llvm::Instruction *, 32> users;

                            auto val = llvm::dyn_cast<llvm::Value>(&instr);
                            llvm::Value::use_iterator sUse = val->use_begin();
                            llvm::Value::use_iterator sEnd = val->use_end();
                            for (; sUse != sEnd; ++sUse) {
                                auto userInstr = llvm::dyn_cast<llvm::Instruction>((*sUse).getUser());
                                if (llvm::isa<llvm::LoadInst>(userInstr)) {
                                    hasLoads = true;
                                    break;
                                } else {
                                    users.push_back(userInstr);
                                }
                            }

                            if (!hasLoads) {
                                toRemove.insert(toRemove.end(), users.begin(), users.end());
                                toRemove.push_back(&instr);
                            }
                        }
                    }
                }

                for (llvm::Instruction *&i: toRemove) {
                    i->eraseFromParent();
                }


                return !toRemove.empty();
            }

        };

    }
}
#endif
