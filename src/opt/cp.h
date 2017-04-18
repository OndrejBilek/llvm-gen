#ifndef MILA_CP_H
#define MILA_CP_H

class AValue{
public:
    enum class Type{
        Top,
        Bottom,
        Const,
        NonZero
    };

    Type type_;
    int value_;

    AValue():
        type_(Type::Bottom),
        value_(0){

    }

    AValue &operator+(AValue const &other) {

    }

    bool mergeWith(AValue const &other) {
        if(type == Type::Const && other.type_ == Type::Const) {
            if (value_ == other.value_) {
                return false;
            } else {
                if (value_ == 0 || other.value_ == 0) {
                    type_ = Type::Top;
                } else {
                    type_ = Type::NonZero;
                }
                return true;
            }
        }

        if (type_ == other.type_ || type_ == Type::Top || other == Type::Bottom){
            return false;
        }

        if (type_ == Type::NonZero && other.type_ == Type::Const || type_ == Type::Const && other.type_ == Type::NonZero) {

        }

        throw "NOT IMPLEMENTED";
    }
};

class AFrame {
public:
    bool mergeWith(AFrame const &other){
        // merge union
        // + add all from other that are not mine
        throw "NOT IMPLEMENTED";
    }

private:
    std::map<llvm::Value *, AValue> values_;
};

class Analysis : public llvm::FunctionPass {
public:

    AFrame * initialState(llvm::Function &f){
        throw "NOT IMPLEMENTED";
    }

    bool run(llvm::Function &f) {
        llvm::BasicBlock *first = firstBlock(f);
        q_.push_back(first);
        initialStates_[first] = initialState(f);

        while(!q_.empty()){
            llvm::BasicBlock *b = q_.pop_front();
            currentState_ = new AFrame(*initialStates_[b]);

            // analyze instructions in the bb, one by one
            for (llvm::Instruction *ins : &b) {
                // add sub mul div - operators any and all
                // load store
                // call
                if (llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(ins)) {
                    currentState[ins] = currentState_[ins->getOperand(0)];
                } else if (llvm::StoreInst *store = llvm::dyn_cast<llvm::StoreInst>(ins)) {
                    currentState[store->getOperand(1)] = currentState_[ins->getOperand(0)];
                } else if (llvm::BinaryOperator *bop = llvm::dyn_cast<llvm::BinaryOperator>(ins)) {
                    AValue lhs = currentState_[bop->getOperand(0)];
                    AValue rhs = currentState_[bop->getOperand(1)];
                    switch(bop->getOpcode()) {
                        case llvm::Instruction::Add:
                            currentState_[ins] = lhs + rhs;
                            break;
                        case llvm::Instruction::Sub:
                        case llvm::Instruction::Mul:
                        case llvm::Instruction::SDiv:
                    }
                }
            }

            // merge outgoing to incoming state of successors and schedule analysis if needed
            for(llvm::BasicBlock *next :: NASLEDNICI){
                AFrame *incoming = initialStates_[next];

                if(incoming == nullptr) {
                    incoming_[next] = new AFrame(*currentState_);
                } else {
                    if (!incoming_->mergeInto(currentState_)){
                        continue;
                    }
                }

                q_.push_back(next);
            }
        }
    }

private:
    AFrame *currentState_;

    std::deque<llvm::BasicBlock *> q_;

    std::map<llvm::BasicBlock *, AFrame *> initialStates_;
};




#endif
