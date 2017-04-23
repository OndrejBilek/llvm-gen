#pragma once

#include <map>
#include <deque>

#include "llvm.h"
#include "mila.h"

namespace mila {

class AValue {
 public:
  enum class Type {
    Top,
    Bottom,
    Const,
    NonZero
  };

  AValue() :
      type_(Type::Bottom) {
  }

  AValue(Type t) :
      type_(t) {
    assert(t != Type::Const && "Use the other constructor");
  }

  AValue(int value) :
      type_(Type::Const),
      value_(value) {
  }

  AValue(AValue const &) = default;

  AValue &operator=(AValue const &) = default;

  Type type() const {
    return type_;
  }

  int value() const {
    assert(type_ == Type::Const);
    return value_;
  }

  bool mergeWith(AValue const &other) {
    // if we are top, or the other guy is bottom, we will definitely not change
    if (isTop() || other.isBottom()) {
      return false;
    }

    // if the two values are the same, do not change as well
    if (*this == other) {
      return false;
    }

    // if the other is top, flip to top
    if (other.isTop()) {
      type_ = Type::Top;
      return true;
    }

    // and finally, if we are bottom, then we are whatever the other guy is
    if (isBottom()) {
      *this = other; // make sure we store the value too
      return true;
    }

    // now deal with the interesting cases
    if (type_ == Type::Const) {

      if (other == Type::Const) {
        assert(value_ != other.value_);

        // different constants - they can't be same by now
        if (value_ == 0 || other.value_ == 0) {
          type_ = Type::Top;
        } else {
          type_ = Type::NonZero;
        }

      } else {
        assert(other == Type::NonZero);
        if (value_ == 0) {
          type_ = Type::Top;
        } else {
          type_ = Type::NonZero;
        }
      }
    } else {
      assert(type_ == Type::NonZero);
      assert(other == Type::Const);
      if (other.value_ != 0) {
        return false;
      }
      type_ = Type::Top;
    }
    return true;
  }

  bool isTop() const {
    return type_ == Type::Top;
  }

  bool isBottom() const {
    return type_ == Type::Top;
  }

  bool isZero() const {
    return type_ == Type::Const && value_ == 0;
  }

  bool isConst() const {
    return type_ == Type::Const;
  }

  bool operator==(AValue const &other) {
    if (type_ != other.type_) {
      return false;
    }
    return type_ == Type::Const ? value_ == other.value_ : true;
  }

  bool operator==(Type t) const {
    return type_ == t;
  }

  bool operator==(int v) const {
    return type_ == Type::Const && value_ == v;
  }

 private:

  Type type_;
  int value_;
};

class AState {
 public:

  AState() = default;

  AValue &operator[](llvm::Value *index) {
    auto i = state_.find(index);
    if (i == state_.end()) {
      // TODO something is missing here that would prevent the analysis from working properly. You will get extra points if you figure out what it is.
      i = state_.insert(std::make_pair(index, AValue())).first;
    }
    return i->second;
  }

  bool mergeWith(AState const &other) {
    bool changed = false;

    // merge all variables from other with ourselves
    for (auto i : other.state_) {

      // ignore bottom values, they can't change what we have
      if (i.second.isBottom()) {
        continue;
      }

      auto j = state_.find(i.first);
      if (j == state_.end()) {
        changed = true;
        // no need to merge if we do not have the variable
        state_.insert(i);
      } else {
        // merge otherwise
        changed = j->second.mergeWith(i.second);
      }
    }
    return changed;
  }

 private:
  std::map<llvm::Value *, AValue> state_;
};

namespace cp {

class Analysis : public llvm::FunctionPass {
 public:

  static char ID;

  Analysis() :
      llvm::FunctionPass(ID) {
  }

  char const *getPassName() const override {
    return "ConstantPropagationAnalysis";
  }

  bool runOnFunction(llvm::Function &f) override {
    // cleanup from previous iteration
    incommingStates_.clear();
    q_.clear();

    // get first basic block and initial state
    llvm::BasicBlock *first = f.begin();
    incommingStates_[first] = initialState(f);

    // schedule the first basic block
    q_.push_back(first);

    // while there are basic blocks to analyze, analyze them
    while (!q_.empty()) {
      // get the basic block and incomming state copy
      setBasicBlock(q_.front());
      q_.pop_front();


      // analyze the instructions
      for (auto i = b_->begin(), e = b_->end(); i != e; ++i) {
        advanceInstruction(i);
      }

      // get the terminator instruction
      llvm::TerminatorInst &tIns = static_cast<llvm::TerminatorInst &>(b_->back());

      // for all successors:
      for (size_t i = 0, e = tIns.getNumSuccessors(); i < e; ++i) {
        llvm::BasicBlock *succ = tIns.getSuccessor(i);

        // merge our state into its incomming and if there is change, schedule the block
        if (incommingStates_[succ].mergeWith(currentState_)) {
          q_.push_back(succ);
        }
      }
    }

    // this is an analysis, it never changes the code
    return false;
  }

 private:
  friend class Optimization;

  AState initialState(llvm::Function &f) const {
    AState result;
    for (auto i = f.arg_begin(), e = f.arg_end(); i != e; ++i) {
      result[i] = AValue::Type::Top;
    }
    return result;
  }

  void setBasicBlock(llvm::BasicBlock *block) {
    b_ = block;
    currentState_ = incommingStates_[b_];
  }

  /** This is where the magic happens.
   */
  void advanceInstruction(llvm::Instruction *ins) {
    if (llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(ins)) {
      currentState_[ins] = currentState_[load->getOperand(0)];
    } else if (llvm::StoreInst *store = llvm::dyn_cast<llvm::StoreInst>(ins)) {
      currentState_[store->getOperand(1)] = currentState_[store->getOperand(0)];
    } else if (llvm::BinaryOperator *bop = llvm::dyn_cast<llvm::BinaryOperator>(ins)) {
      AValue lhs = currentState_[bop->getOperand(0)];
      AValue rhs = currentState_[bop->getOperand(1)];
      assert(!lhs.isBottom());
      assert(!rhs.isBottom());
      if (lhs.isConst() && rhs.isConst()) {
        switch (bop->getOpcode()) {
          case llvm::Instruction::Add:currentState_[ins] = lhs.value() + rhs.value();
            break;
            // TODO I am homework
          default:UNREACHABLE;
            break;
        }
      } else {
        currentState_[ins] = AValue::Type::Top;
      }
    } else if (llvm::ICmpInst *cmp = llvm::dyn_cast<llvm::ICmpInst>(ins)) {
      AValue lhs = currentState_[bop->getOperand(0)];
      AValue rhs = currentState_[bop->getOperand(1)];
      assert(!lhs.isBottom());
      assert(!rhs.isBottom());
      if (lhs.isConst() && rhs.isConst()) {
        switch (cmp->getSignedPredicate()) {
          case llvm::ICmpInst::ICMP_EQ:currentState_[ins] = lhs.value() == rhs.value();
            break;
            // TODO I am homework
          default:UNREACHABLE;
            break;
        }
      } else {
        currentState_[ins] = AValue::Type::Top;
      }
      // TODO I am homework - what other instructions do we need to care about? Do we have to do something with those we do not care about wrt cp analysis?
    }
  }

  llvm::BasicBlock *b_;

  AState currentState_;
  std::map<llvm::BasicBlock *, AState> incommingStates_;
  std::deque<llvm::BasicBlock *> q_;
};

class Optimization : public llvm::FunctionPass {
 public:

  static char ID;

  Optimization() :
      llvm::FunctionPass(ID) {
  }

  char const *getPassName() const override {
    return "ConstantPropagationOptimization";
  }

  void getAnalysisUsage(llvm::AnalysisUsage &au) const {
    au.addRequired<Analysis>();
  }

  bool runOnFunction(llvm::Function &f) override {
    // We'll do this on next lecture
    return false;
  }

};

}

}

