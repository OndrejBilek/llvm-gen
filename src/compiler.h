#ifndef COMPILER_H
#define COMPILER_H

#include "llvm.h"

#include "mila/ast.h"

namespace mila {

class CompilerError : public Exception {
 public:
  CompilerError(std::string const &what, ast::Node const *ast) :
      Exception(STR(what << " (line: " << ast->line << ", col: " << ast->col << ")")) {
  }

  CompilerError(std::string const &what) :
      Exception(what) {
  }
};

/** Compiler */
class Compiler : public ast::Visitor {
 private:

  class Location {
   public:
    static Location variable(llvm::Value *address) {
      return Location(address, false);
    }

    static Location constant(llvm::Value *value) {
      return Location(value, true);
    }

    bool isConstant() const {
      return isConstant_;
    }

    llvm::Value *value() const {
      assert (isConstant_);
      return location_;
    }

    llvm::Value *address() const {
      assert (not isConstant_);
      return location_;
    }

    Location(Location const &) = default;

    Location &operator=(Location const &) = default;

    Location() :
        location_(nullptr),
        isConstant_(false) {
    }

    Location(llvm::Value *location, bool isConstant) :
        location_(location),
        isConstant_(isConstant) {
    }

   private:

    llvm::Value *location_;
    bool isConstant_;
  };

  /** Compilation context for a block. */
  class BlockContext {
   public:

    bool hasVariable(Symbol symbol) const {
      return variables.find(symbol) != variables.end();
    }

    Location const &get(Symbol symbol, ast::Node *ast) {
      auto i = variables.find(symbol);
      if (i != variables.end())
        return i->second;
      if (parent != nullptr)
        return parent->get(symbol, ast);
      throw CompilerError(STR("Variable or constant " << symbol << " not found"), ast);
    }

    std::map<Symbol, Location> variables;

    BlockContext *parent;

    BlockContext(BlockContext *parent = nullptr) :
        parent(parent) {
    }

  };

  llvm::Module *m;

  llvm::Function *f;

  llvm::BasicBlock *bb;

  BlockContext *c;

  llvm::Value *result;

  static llvm::Type *t_int;
  static llvm::Type *t_void;

  static llvm::FunctionType *t_read;
  static llvm::FunctionType *t_write;

  static llvm::Value *zero;
  static llvm::Value *one;

  static llvm::LLVMContext &context;

 public:
  static llvm::Function *compile(ast::Module *module) {
    Compiler c;
    module->accept(&c);

    // check that the module's IR is well formed
    llvm::raw_os_ostream err(std::cerr);
    if (llvm::verifyModule(*c.m, &err)) {
      c.m->dump();
      throw CompilerError("Invalid LLVM bitcode produced");
    }

    // return the main function
    return c.f;
  }

 protected:
  virtual void visit(ast::Node *n) {
    throw Exception("Unknown compiler handler");
  }

  virtual void visit(ast::Expression *d) {
    throw Exception("Unknown compiler handler");
  }

  void compileDeclarations(ast::Declarations *ds, bool isGlobal = false) {
    for (ast::Declaration *d : ds->declarations) {

      if (c->hasVariable(d->symbol)) {
        throw CompilerError(STR("Redefinition of variable " << d->symbol), d);
      }

      if (d->value == nullptr) {
        if (isGlobal) {
          auto gv = new llvm::GlobalVariable(*m,
                                             t_int,
                                             false,
                                             llvm::GlobalValue::CommonLinkage,
                                             nullptr,
                                             d->symbol.name() + "_");
          gv->setAlignment(4);
          gv->setInitializer(llvm::ConstantInt::get(context, llvm::APInt(32, 0)));
          c->variables[d->symbol] = Location::variable(gv);
        } else {
          c->variables[d->symbol] = Location::variable(new llvm::AllocaInst(t_int, d->symbol.name(), bb));
        }
      } else {
        d->value->accept(this);
        c->variables[d->symbol] = Location::constant(result);
      }
    }
  }

  virtual void visit(ast::Declaration *d) {
    throw Exception("This should be unreachable");
  }

  virtual void visit(ast::Function *f) {
    if (f->name == "main") {
      throw CompilerError("Cannot create user defined main function", f);
    }

    std::vector<llvm::Type *> at;
    for (size_t i = 0, e = f->arguments.size(); i != e; ++i) {
      at.push_back(t_int);
    }
    llvm::FunctionType *ft = llvm::FunctionType::get(t_int, at, false);

    if (m->getFunction(f->name.name()) != nullptr) {
      throw CompilerError(STR("Function " << f->name << " already exists"), f);
    }
    this->f = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage, f->name.name(), m);
    this->f->setCallingConv(llvm::CallingConv::C);

    c = new BlockContext(c);
    bb = llvm::BasicBlock::Create(context, "", this->f);

    llvm::Function::arg_iterator args = this->f->arg_begin();
    for (Symbol const &s: f->arguments) {
      llvm::Value *v = args++;

      if (c->hasVariable(s)) {
        throw CompilerError(STR("Redefinition of variable " << s), f);
      }

      llvm::AllocaInst *loc = new llvm::AllocaInst(t_int, s.name(), bb);
      c->variables[s] = Location::variable(loc);
      new llvm::StoreInst(v, loc, false, bb);

      v->setName(s.name());
      loc->setName(s.name());
    }

    compileFunctionBody(f->body);
    BlockContext *x = c;
    c = c->parent;
    delete x;
  }

  void compileFunctionBody(ast::Node *node) {
    node->accept(this);

    if (result == nullptr) {
      result = llvm::ReturnInst::Create(context, zero, bb);
    } else if (not llvm::isa<llvm::ReturnInst>(result)) {
      result = llvm::ReturnInst::Create(context, result, bb);
    }
  }

  virtual void visit(ast::Functions *fs) {
    for (ast::Function *f : fs->functions) {
      f->accept(this);
    }
  }

  virtual void visit(ast::Declarations *ds) {
    compileDeclarations(ds);
  }

  virtual void visit(ast::Module *module) {
    result = nullptr;
    m = new llvm::Module("mila", context);

    llvm::Function::Create(t_read,
                           llvm::GlobalValue::ExternalLinkage,
                           "read_",
                           m)->setCallingConv(llvm::CallingConv::C);
    llvm::Function::Create(t_write,
                           llvm::GlobalValue::ExternalLinkage,
                           "write_",
                           m)->setCallingConv(llvm::CallingConv::C);

    c = new BlockContext(nullptr);
    compileDeclarations(module->declarations, true);

    module->functions->accept(this);

    llvm::FunctionType *ft = llvm::FunctionType::get(t_int, false);
    f = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage, "main", m);

    bb = llvm::BasicBlock::Create(context, "", this->f);
    compileFunctionBody(module->body);
  }

  virtual void visit(ast::Block *d) {
    c = new BlockContext(c);
    d->declarations->accept(this);

    for (ast::Node *s : d->statements) {
      if (bb == nullptr) {
        throw CompilerError("Code after return statement is not allowed", s);
      }
      s->accept(this);
    }

    BlockContext *x = c;
    c = c->parent;
    delete x;
  }

  virtual void visit(ast::Write *w) {
    w->expression->accept(this);
    llvm::CallInst::Create(m->getFunction("write_"), result, "", bb);
  }

  virtual void visit(ast::Read *r) {
    result = llvm::CallInst::Create(m->getFunction("read_"), r->symbol.name(), bb);

    Location const &l = c->get(r->symbol, r);
    if (l.isConstant()) {
      throw CompilerError(STR("Cannot assign constant " << r->symbol), r);
    }

    new llvm::StoreInst(result, l.address(), false, bb);
  }

  virtual void visit(ast::If *s) {
    s->condition->accept(this);

    llvm::BasicBlock *trueCase = llvm::BasicBlock::Create(context, "trueCase", f);
    llvm::BasicBlock *falseCase = llvm::BasicBlock::Create(context, "falseCase", f);
    llvm::BasicBlock *next = llvm::BasicBlock::Create(context, "next", f);

    llvm::ICmpInst *cmp = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_NE, result, zero, "");
    llvm::BranchInst::Create(trueCase, falseCase, cmp, bb);

    bb = trueCase;
    s->trueCase->accept(this);
    trueCase = bb;
    llvm::Value *trueResult = result;
    if (trueCase != nullptr) {
      llvm::BranchInst::Create(next, bb);
    }

    bb = falseCase;
    s->falseCase->accept(this);
    falseCase = bb;
    llvm::Value *falseResult = result;
    if (falseCase != nullptr) {
      llvm::BranchInst::Create(next, bb);
    }

    bb = next;
    if (falseCase == nullptr and trueCase == nullptr) {
      next->eraseFromParent();
      result = nullptr;
    } else {
      llvm::PHINode *phi = llvm::PHINode::Create(t_int, 2, "if_phi", bb);
      if (trueCase != nullptr) {
        phi->addIncoming(trueResult, trueCase);
      }
      if (falseCase != nullptr) {
        phi->addIncoming(falseResult, falseCase);
      }
      result = phi;
    }
  }

  virtual void visit(ast::While *d) {
    // TODO I am homework
    llvm::BasicBlock *cond = llvm::BasicBlock::Create(context, "while_cond", f);
    llvm::BasicBlock *body = llvm::BasicBlock::Create(context, "while_body", f);
    llvm::BasicBlock *next = llvm::BasicBlock::Create(context, "while_next", f);

    llvm::BranchInst::Create(cond, bb);

    bb = cond;
    d->condition->accept(this);
    llvm::ICmpInst *cmp = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_NE, result, zero, "");
    llvm::BranchInst::Create(body, next, cmp, bb);

    bb = body;
    d->body->accept(this);
    if (bb != nullptr) {
      llvm::BranchInst::Create(cond, bb);
    }

    bb = next;
    result = nullptr;
  }

  virtual void visit(ast::Return *r) {
    // TODO I am homework
    r->value->accept(this);
    result = llvm::ReturnInst::Create(context, result, bb);
    bb = nullptr;
  }

  virtual void visit(ast::Assignment *a) {
    a->value->accept(this);

    Location const &l = c->get(a->symbol, a);
    if (l.isConstant()) {
      throw CompilerError(STR("Cannot assign constant " << a->symbol), a);
    }

    new llvm::StoreInst(result, l.address(), false, bb);
  }

  virtual void visit(ast::Call *call) {
    std::vector<llvm::Value *> args;
    for (ast::Node *a : call->arguments) {
      a->accept(this);
      args.push_back(result);
    }

    llvm::Function *f = m->getFunction(call->function.name());
    if (f == nullptr) {
      throw CompilerError(STR("Call to undefined function " << call->function), call);
    }

    if (f->arg_size() != args.size()) {
      throw CompilerError(STR("Function " << call->function << " declared with different number of arguments"), call);
    }

    result = llvm::CallInst::Create(f, args, call->function.name(), bb);
  }

  virtual void visit(ast::Binary *op) {
    // TODO I am homework
    op->lhs->accept(this);
    llvm::Value *resultLhs = result;

    op->rhs->accept(this);
    llvm::Value *resultRhs = result;

    switch (op->type) {
      case Token::Type::opAdd:result = llvm::BinaryOperator::CreateAdd(resultLhs, resultRhs, "add", bb);
        return;
      case Token::Type::opSub:result = llvm::BinaryOperator::CreateSub(resultLhs, resultRhs, "sub", bb);
        return;
      case Token::Type::opMul:result = llvm::BinaryOperator::CreateMul(resultLhs, resultRhs, "mul", bb);
        return;
      case Token::Type::opDiv:result = llvm::BinaryOperator::CreateSDiv(resultLhs, resultRhs, "div", bb);
        return;
      case Token::Type::opEq:result = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_EQ, resultLhs, resultRhs, "eq");
        break;
      case Token::Type::opNeq:result = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_NE, resultLhs, resultRhs, "ne");
        break;
      case Token::Type::opLt:result = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_SLT, resultLhs, resultRhs, "lt");
        break;
      case Token::Type::opGt:result = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_SGT, resultLhs, resultRhs, "gt");
        break;
      case Token::Type::opLte:result = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_SLE, resultLhs, resultRhs, "le");
        break;
      case Token::Type::opGte:result = new llvm::ICmpInst(*bb, llvm::ICmpInst::ICMP_SGE, resultLhs, resultRhs, "ge");
        break;
      default:UNREACHABLE;
    }

    result = new llvm::ZExtInst(result, t_int, "", bb);
  }

  virtual void visit(ast::Unary *op) {
    // TODO I am homework
    op->operand->accept(this);

    switch (op->type) {
      case Token::Type::opAdd:result = llvm::BinaryOperator::CreateAdd(result, one, "inc", bb);
        break;
      case Token::Type::opSub:result = llvm::BinaryOperator::CreateSub(result, one, "dec", bb);
        break;
      default:UNREACHABLE;
    }
  }

  virtual void visit(ast::Variable *v) {
    Location const &l = c->get(v->symbol, v);
    if (l.isConstant()) {
      result = l.value();
    } else {
      result = new llvm::LoadInst(l.address(), v->symbol, bb);
    }
  }

  virtual void visit(ast::Number *n) {
    result = llvm::ConstantInt::get(context, llvm::APInt(32, n->value));
  }

};

}
#endif
