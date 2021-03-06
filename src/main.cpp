/* main.c */
/* syntakticky analyzator */

#include <cstdlib>
#include <iostream>
#include <tests/tests.h>

#include "llvm.h"

#include "mila/scanner.h"
#include "mila/ast.h"
#include "mila/parser.h"
#include "mila/printer.h"
#include "compiler.h"
#include "jit.h"

using namespace mila;

int main(int argc, char const *argv[]) {
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

  //tests();
  JIT::optimize = true;

  try {
    char const *filename = nullptr;
    bool verbose = false;
    char const *emitir = nullptr;

    for (int i = 1; i < argc; ++i) {
      if (strncmp(argv[i], "--verbose", 10) == 0) {
        verbose = true;
      } else if (strncmp(argv[i], "--emit", 7) == 0) {
        emitir = argv[++i];
      } else if (filename != nullptr) {
        throw Exception("Invalid usage! mila+ [--verbose] [--emit filename] filename");
      } else {
        filename = argv[i];
      }
    }

    ast::Module *m = Parser::parse(Scanner::file(filename));
    if (verbose) {
      std::cout << "###### INPUT FILE ######" << std::endl;
      ast::Printer::print(m);
    }

    llvm::Function *f = Compiler::compile(m);
    if (verbose) {
      std::cout << "###### PRE-JIT ######" << std::endl;
      f->getParent()->dump();
    }

    if (emitir != nullptr) {
      std::error_code error;
      llvm::raw_fd_ostream o(emitir, error, llvm::sys::fs::OpenFlags::F_None);
      llvm::WriteBitcodeToFile(f->getParent(), o);
    } else {
      JIT::compile(f)();
      if (verbose) {
        std::cout << "###### POST-JIT ######" << std::endl;
        f->getParent()->dump();
      }
    }

    return EXIT_SUCCESS;
  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
