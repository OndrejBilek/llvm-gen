#include "compiler.h"

namespace mila {
llvm::Type *Compiler::t_int = llvm::IntegerType::get(context, 32);

llvm::Type *Compiler::t_void = llvm::Type::getVoidTy(context);

llvm::FunctionType *Compiler::t_read = llvm::FunctionType::get(t_int, false);

llvm::FunctionType *Compiler::t_write = llvm::FunctionType::get(t_void, {t_int}, false);

llvm::Value *Compiler::zero = llvm::ConstantInt::get(context, llvm::APInt(32, 0));

llvm::Value *Compiler::one = llvm::ConstantInt::get(context, llvm::APInt(32, 1));
}
