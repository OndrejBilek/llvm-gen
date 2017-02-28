#include <cstdlib>
#include <iostream>

#include "llvm.h"

int main(int argc, char * argv []) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    return EXIT_SUCCESS;
}