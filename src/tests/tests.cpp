#include "mila.h"
#include "llvm.h"

#include "mila/scanner.h"
#include "mila/ast.h"
#include "mila/parser.h"
#include "compiler.h"
#include "jit.h"

namespace mila {

std::string printBlock(llvm::BasicBlock * b) {
  std::string result = "B";
  for (llvm::Instruction & i : *b) {
    if (llvm::BranchInst * bi = llvm::dyn_cast<llvm::BranchInst>(&i)) {
      if (bi->isConditional()) {
        result += " cbr";
        continue;
      }
    }
    result = result + " " + i.getOpcodeName();
    if (llvm::ICmpInst * cmp = llvm::dyn_cast<llvm::ICmpInst>(&i)) {
      switch (cmp->getSignedPredicate()) {
        case llvm::ICmpInst::ICMP_EQ:
          result += " eq";
          break;
        case llvm::ICmpInst::ICMP_NE:
          result += " ne";
          break;
        case llvm::ICmpInst::ICMP_SLT:
          result += " slt";
          break;
        case llvm::ICmpInst::ICMP_SGT:
          result += " sgt";
          break;
        case llvm::ICmpInst::ICMP_SLE:
          result += " sle";
          break;
        case llvm::ICmpInst::ICMP_SGE:
          result += " sge";
          break;
        default:
          result += " ??";
      }
    }
  }
  return result;
}

std::string printFunction(llvm::Function * f) {
  if (f == nullptr)
    return "FUNCTION NOT FOUND";
  std::string result;
  for (llvm::BasicBlock & b : *f) {
    if (not result.empty())
      result += " ";
    result += printBlock(&b);
  }
  return result;
}


#define TEST(code) Test(__FILE__, __LINE__, code)

class Test {
 public:


  Test(char const * file, int line, std::string const & code):
      file_(file),
      line_(line),
      ast_(nullptr),
      main_(nullptr),
      jit_(nullptr)
  {
    try {
      ast_ = Parser::parse(Scanner::text(code + "\n begin f() end"));
      main_ = Compiler::compile(ast_);
      jit_ = JIT::compile(main_);
      ++points_;
    } catch (ParserError const & e) {
      printLocation();
      std::cerr << "  Compilation failed:" << std::endl;
      std::cerr << e.what() << std::endl;
      ++failures_;
    } catch (CompilerError const & e) {
      printLocation();
      std::cerr << "  Compilation failed:" << std::endl;
      std::cerr << e.what() << std::endl;
      ++failures_;
    } catch (...) {
      printLocation();
      std::cerr << "  Compilation failed." << std::endl;
      ++failures_;
    }
  }

  Test & run(int expected) {
    if (jit_ != nullptr) {
      try {
        //main_->getParent()->dump();
        int actual = jit_();
        if (actual != expected) {
          printLocation();
          std::cerr << "  Run failed, expected " << expected << ", got " << actual << std::endl;
          ++failures_;
          /*for (llvm::Function & f : *(main_->getParent()))
               f.dump(); */


        } else {
          ++points_;
        }
      } catch (...) {
        printLocation();
        std::cerr << "  Run failed." << std::endl;
        ++failures_;
      }
    }
    return *this;
  }

  Test & blocks(size_t expected, char const * name = "f") {
    if (main_ != nullptr) {
      llvm::Function * f = main_->getParent()->getFunction(name);
      if (f->size() != expected) {
        printLocation();
        std::cerr << "  Function " << name << " expected to have " << expected << " basic blocks, but has " << f->size() << std::endl;
        ++failures_;
      } else {
        ++points_;
      }


    }
    return *this;
  }

  Test & insns(int expected) {
    return *this;
  }

  Test & code(std::string const & expected, char const * name = "f") {
    if (main_ != nullptr) {
      try {
        llvm::Function * f = main_->getParent()->getFunction(name);
        std::string actual = printFunction(f);
        size_t x = actual.find(expected);
        bool result = true;
        if (x == std::string::npos)
          result = false;
        if (x > 0) {
          actual = actual.substr(0, x);
          if (actual.find("B ") == 0)
            actual = actual.substr(2);
          while (actual.find("alloca store ") == 0)
            actual = actual.substr(13);
          if (actual.find("br ") == 0)
            actual = actual.substr(3);
          if (actual.find("B ") == 0)
            actual = actual.substr(2);
          if (actual.size() != 0)
            result = false;
        }
        if (not result) {
          printLocation();
          std::cerr << "  Code comparison for function " << name << " failed, expected " << expected << ", got " << printFunction(f) << std::endl;
          ++failures_;
        } else {
          ++points_;
        }
      } catch (...) {
        printLocation();
        std::cerr << "  Code comparison for function " << name << " failed";
        ++failures_;
      }
    }
    return *this;
  }

  Test & containsNot(std::string const & expected, char const * name = "f") {
    if (main_ != nullptr) {
      try {
        llvm::Function * f = main_->getParent()->getFunction(name);
        std::string actual = printFunction(f);
        if (actual.find(expected) != std::string::npos) {
          printLocation();
          std::cerr << "  Code for function  " << name << " contains " << expected << ", in actual code " << actual << std::endl;
          ++failures_;
        } else {
          ++points_;
        }
      } catch (...) {
        printLocation();
        std::cerr << "  Code conetnts check for function " << name << " failed";
        ++failures_;
      }
    }
    return *this;
  }

  Test & containsSingle(std::string const & expected, char const * name = "f") {
    if (main_ != nullptr) {
      try {
        llvm::Function * f = main_->getParent()->getFunction(name);
        std::string actual = printFunction(f);
        size_t first = actual.find(expected);
        if (first == std::string::npos) {
          printLocation();
          std::cerr << "  Code for function  " << name << " does not contain " << expected << ", in actual code " << actual << std::endl;
          ++failures_;
        }
        if (actual.find(expected, first + 1) != std::string::npos) {
          printLocation();
          std::cerr << "  Code for function  " << name << " contains " << expected << " more than once, in actual code " << actual << std::endl;
          ++failures_;
        } else {
          ++points_;
        }
      } catch (...) {
        printLocation();
        std::cerr << "  Code conetnts check for function " << name << " failed";
        ++failures_;
      }
    }
    return *this;
  }


  static void stats() {
    std::cout << "Finished, total points: " << points_ << std::endl;
    std::cout << "              Failures: " << failures_ << std::endl;
    std::cout << "              Total: " << failures_ + points_ << std::endl;
  }

  ~Test() {
    //delete ast_;
    //delete main_;
  }

 private:


  void printLocation() {
    if (not error_) {
      error_ = true;
      std::cerr << "ERROR: " << file_ << " [" << line_ << "]:" << std::endl;
    }
  }

  char const * file_;
  int line_;

  ast::Module * ast_;
  llvm::Function * main_;
  JIT::MainPtr jit_;

  bool error_ = false;


  static int points_;
  static int failures_;






};

int Test::points_ = 0;
int Test::failures_ = 0;




void test_lowering_precise() {
  JIT::optimize = false;
  std::cout << "Lowering (precission tests)..." << std::endl;
  TEST("function f() 1")
      .run(1)
      .code("B ret");
  TEST("function f() 2 + 3")
      .run(5)
      .code("B add ret");
  TEST("function f() return 2 + 5")
      .run(7)
      .code("B add ret");
  TEST("function f() begin 2 + 5 end")
      .run(7)
      .code("B add ret");
  TEST("function f() begin return 2 + 5 end")
      .run(7)
      .code("B add ret");
  TEST("function f() begin var a; var b; a := 2; b := 5; a + b; end")
      .run(7)
      .code("B alloca alloca store store load load add ret");
  TEST("function f() 2 - 3")
      .run(-1)
      .code("B sub ret");
  TEST("function f() 2 * 3")
      .run(6)
      .code("B mul ret");
  TEST("function f() 10 / 2")
      .run(5)
      .code("B sdiv ret");
  TEST("function f() 10 / 3")
      .run(3)
      .code("B sdiv ret");
  TEST("function f() 2 = 2")
      .run(1)
      .code("B icmp eq zext ret");
  TEST("function f() 2 = 3")
      .run(0)
      .code("B icmp eq zext ret");
  TEST("function f() 2 <> 2")
      .run(0)
      .code("B icmp ne zext ret");
  TEST("function f() 2 <> 3")
      .run(1)
      .code("B icmp ne zext ret");
  TEST("function f() 1 < 2")
      .run(1)
      .code("B icmp slt zext ret");
  TEST("function f() 4 < 3")
      .run(0)
      .code("B icmp slt zext ret");
  TEST("function f() 2 <= 2")
      .run(1)
      .code("B icmp sle zext ret");
  TEST("function f() 2 >= 2")
      .run(1)
      .code("B icmp sge zext ret");
  TEST("function f() 1 > 2")
      .run(0)
      .code("B icmp sgt zext ret");
  TEST("function f() 4 > 3")
      .run(1)
      .code("B icmp sgt zext ret");
  TEST("function t4(a, b) a + b; function f() t4(1,2)")
      .run(3)
      .code("B call ret")
      .code("B alloca store alloca store load load add ret", "t4");
  TEST("function f() begin var i; i := 678; return i; end")
      .run(678)
      .code("B alloca store load ret");
  TEST("function t6(i, j) begin if (i < j) then return i; else return j; end function f() t6(5,2)")
      .run(2)
      .code("B call ret")
      .code("B alloca store alloca store load load icmp slt sext icmp ne br B load ret B load ret", "t6");
  TEST("function t6(i, j) begin if (i < j) then return i; else return j; end function f() t6(3,10)")
      .run(3)
      .code("B call ret")
      .code("B alloca store alloca store load load icmp slt sext icmp ne br B load ret B load ret", "t6");
  TEST("function t7(i, j) begin if (i < j) then return i; else return 4; end function f() t7(3,10)")
      .run(3)
      .code("B call ret")
      .code("B alloca store alloca store load load icmp slt sext icmp ne br B load ret B ret", "t7");
  TEST("function t7(i, j) begin if (i < j) then return i; else return 4; end function f() t7(10,2)")
      .run(4)
      .code("B call ret")
      .code("B alloca store alloca store load load icmp slt sext icmp ne br B load ret B ret", "t7");
  TEST("function f() begin var i; i := 10; while (i > 0) do begin i := i - 1; return 67; end end")
      .run(67)
      .code("B alloca store br B load icmp sgt sext icmp ne br B load sub store ret B ret");
  TEST("function f() begin var i; i := 10; begin var i; i := 12; return i; end end")
      .run(12)
      .code("B alloca store alloca store load ret");
  TEST("function f() begin var i; i := 10; begin var i; i := 12; end return i end")
      .run(10)
      .code("B alloca store alloca store load ret");
  TEST("function f() i; var i;")
      .code("B load ret");
  TEST("function f() begin var i, b; i := 10; b := 1; while (i <> 0) do begin b := b * 2; i := i - 1; end return b; end")
      .run(1024)
      .code("B alloca alloca store store br B load icmp ne sext icmp ne br B load mul store load sub store br B load ret");
  TEST("function f() 2 + 3 * 4")
      .run(14)
      .code("B mul add ret");
  JIT::optimize = true;
}

void test_lowering() {
  std::cout << "Lowering tests..." << std::endl;
  TEST("function f() 1")
      .run(1);
  TEST("function f() 2 + 3")
      .run(5);
  TEST("function f() return 2 + 5")
      .run(7);
  TEST("function f() begin 2 + 5 end")
      .run(7);
  TEST("function f() begin return 2 + 5 end")
      .run(7);
  TEST("function f() begin var a; var b; a := 2; b := 5; a + b; end")
      .run(7);
  TEST("function f() 2 - 3")
      .run(-1);
  TEST("function f() 2 * 3")
      .run(6);
  TEST("function f() 10 / 2")
      .run(5);
  TEST("function f() 10 / 3")
      .run(3);
  TEST("function f() 2 = 2")
      .run(1);
  TEST("function f() 2 = 3")
      .run(0);
  TEST("function f() 2 <> 2")
      .run(0);
  TEST("function f() 2 <> 3")
      .run(1);
  TEST("function f() 1 < 2")
      .run(1);
  TEST("function f() 4 < 3")
      .run(0);
  TEST("function f() 2 <= 2")
      .run(1);
  TEST("function f() 2 >= 2")
      .run(1);
  TEST("function f() 1 > 2")
      .run(0);
  TEST("function f() 4 > 3")
      .run(1);
  TEST("function t4(a, b) a + b; function f() t4(1,2)")
      .run(3);
  TEST("function f() begin var i; i := 678; return i; end")
      .run(678);
  TEST("function t6(i, j) begin if (i < j) then return i; else return j; end function f() t6(5,2)")
      .run(2);
  TEST("function t6(i, j) begin if (i < j) then return i; else return j; end function f() t6(3,10)")
      .run(3);
  TEST("function t7(i, j) begin if (i < j) then return i; else return 4; end function f() t7(3,10)")
      .run(3);
  TEST("function t7(i, j) begin if (i < j) then return i; else return 4; end function f() t7(10,2)")
      .run(4);
  TEST("function f() begin var i; i := 10; while (i > 0) do begin i := i - 1; return 67; end end")
      .run(67);
  TEST("function f() begin var i; i := 10; begin var i; i := 12; return i; end end")
      .run(12);
  TEST("function f() begin var i; i := 10; begin var i; i := 12; end return i end")
      .run(10);
  TEST("function f() begin var i, b; i := 10; b := 1; while (i <> 0) do begin b := b * 2; i := i - 1; end return b; end")
      .run(1024);
  TEST("function f() 2 + 3 * 4")
      .run(14);
}

void test_simpleCP() {
  std::cout << "Simple constant propagation..." << std::endl;
  TEST("function f() 2 + 3")
      .run(5)
      .code("ret");
  TEST("function f() 2 - 3")
      .run(-1)
      .code("ret");
  TEST("function f() 4 * 3")
      .run(12)
      .code("ret");
  TEST("function f() 10 / 5")
      .run(2)
      .code("ret");
  TEST("function f() 1 = 2")
      .run(0)
      .code("ret");
  TEST("function f() 1 = 1")
      .run(1)
      .code("ret");
  TEST("function f() 1 <> 2")
      .run(1)
      .code("ret");
  TEST("function f() 1 <> 1")
      .run(0)
      .code("ret");
  TEST("function f() 1 < 2")
      .run(1)
      .code("ret");
  TEST("function f() 1 < 1")
      .run(0)
      .code("ret");
  TEST("function f() 1 <= 2")
      .run(1)
      .code("ret");
  TEST("function f() 1 <= 1")
      .run(1)
      .code("ret");
  TEST("function f() 2 <= 1")
      .run(0)
      .code("ret");
  TEST("function f() 1 >= 2")
      .run(0)
      .code("ret");
  TEST("function f() 1 >= 1")
      .run(1)
      .code("ret");
  TEST("function f() 2 >= 1")
      .run(1)
      .code("ret");
  TEST("function f() 1 + 2 + 3")
      .run(6)
      .code("ret");
  TEST("function f() 1 + 2 = 3")
      .run(1)
      .code("B ret");
}

void test_cp() {
  std::cout << "Constant propagation..." << std::endl;
  TEST("function f() begin var a, b; a := 1; b := a + 1; a := 2; b end").run(2).containsNot("load");
  TEST("function f() begin var a, b; a := 1; b := 2; if (1) then a; else b; end")
      .run(1)
      .containsNot("load");
  TEST("function f() begin var a, b; a := 1; b := 2; if (0) then a; else b; end")
      .run(2)
      .containsNot("load");
  TEST("function fx(x) begin var a, b; a := 1; b := 2; if (x) then a; else b; end function f() fx(1)")
      .run(1)
      .containsSingle("load", "fx");
  TEST("function fx(x) begin var a, b; a := 1; b := 2; if (x) then a; else b; end function f() fx(0)")
      .run(2)
      .containsSingle("load", "fx");


}

void test_die() {
  std::cout << "Dead instruction elimination..." << std::endl;
  TEST("function f() begin 2 + 2; 3; end")
      .run(3)
      .code("ret");
  TEST("function g() begin h := 3; end function f() begin g(); 3; end var h")
      .run(3)
      .code("call ret");
  TEST("function f() begin var a; a; 2; end")
      .run(2)
      .containsNot("load");
  TEST("function f() begin 1; 2; 3; end")
      .run(3)
      .code("ret");
  TEST("function f() begin var a; var b; a := 1; b := 2; b + 1; end")
      .run(3)
      .containsNot("load");
}

void test_dce() {
  std::cout << "Dead code elimination..." << std::endl;
  TEST("function f() begin if (0) then return 1 else return 2 end").run(2).containsNot("cbr");
  TEST("function f() begin const a = 1 if (a) then return 1 else return 2 end").run(1).containsNot("cbr");
  TEST("function f() begin var a a := 1 if (a) then return 1 else return 2 end").run(1).containsNot("cbr");
  TEST("function f() begin while (0) do 1 return 1 end").run(1).containsNot("cbr");
  //TEST("function ff(a) begin if(a) then if (0) then return 1 else return 0 else return 3 end function f() ff(1)").run(0).containsSingle("cbr", "ff");

}

void test_peephole() {
  std::cout << "Peepholer..." << std::endl;
  TEST("function ff(a) a * 4 function f() ff(10)")
      .run(40)
      .code("load shl ret", "ff");
  TEST("function ff(a) 8 * a function f() ff(10)")
      .run(80)
      .code("load shl ret", "ff");
  TEST("function ff(a) a / 16 function f() ff(32)")
      .run(2)
      .code("load ashr ret", "ff");
  TEST("function ff(a) a + a function f() ff(10)")
      .run(20)
      .code("load shl ret", "ff");
  TEST("function ff(a) (a + 2) + (a + 2) function f() ff(10)")
      .run(24)
      .code("load add shl ret", "ff");
  TEST("function ff(a) (a + 2) + (2 + a) function f() ff(10)")
      .run(24)
      .code("load add shl ret", "ff");
  TEST("function ff(a) (a + a) + (a * 2) function f() ff(10)")
      .run(40)
      .code("load shl shl ret", "ff");
}

void test_inlining() {
  std::cout << "Inlining..." << std::endl;
  TEST("function fx() 1 function f() while (1) do begin var a; a := fx(); if (a) then return a end")
      .run(1)
      .containsNot("call");
  TEST("function fx(a, b) a - b function f() while (1) do begin var a; a := fx(10,3); if (a) then return a end")
      .run(7)
      .containsNot("call");
  TEST("function fx(a, b) if (a > b) then a else b function f() while (1) do begin var a; a := fx(10,3); if (a) then return a end")
      .run(10)
      .containsNot("call");
  TEST("function fx(a, b) if (a > b) then a else b function f() while (1) do begin var a; a := fx(10,30); if (a) then return a end")
      .run(30)
      .containsNot("call");
  TEST("function fx(a, b) if (a > b) then a else b function f() while (1) do begin var a; a := fx(1,3) + fx(7, 2); if (a) then return a end")
      .run(10)
      .containsNot("call");
  TEST("function fx() while (1) do fx() function f() 1")
      .run(1)
      .containsSingle("call","fx");
  TEST("function fx() 1 function f() fx()")
      .run(1)
      .containsSingle("call");
}

void test_unrolling() {
  std::cout << "Loop unrolling..." << std::endl;
}


void test_tailRecursion() {
  std::cout << "Tail Recursion..." << std::endl;
  TEST("function fact(a) begin if (a = 0) then return 1 else begin a := a - 1; end return fact(a) end function f() fact(10)")
      .run(1)
      .containsNot("call", "fact");
  TEST("function fact(a) begin if (a = 0) then return 1 else return fact(a - 1); end function f() fact(10)")
      .run(1)
      .containsNot("call", "fact");

  TEST("function fact(r, x) begin if (x = 0) then return r; else begin r := r * x; x := x - 1; return fact(r, x); end end function f() fact(1, 10)")
      .run(3628800)
      .containsNot("call", "fact");

  TEST("function fact(r, x) begin if (x = 0) then return r; else return fact(r * x, x - 1); end function f() fact(1, 10)")
      .run(3628800)
      .containsNot("call", "fact");

  TEST("function nota(x) begin if (x > 0) then nota(x - 1); x; end function f() nota(10)")
      .run(10)
      .containsSingle("call","nota");
}




void tests() {
  //test_lowering_precise();
  //test_lowering();
  //test_simpleCP();
  //test_cp();
  test_die();
  test_dce();
  //test_peephole();
  //test_inlining();
  //test_unrolling();
  //test_tailRecursion();

  Test::stats();
}

}


