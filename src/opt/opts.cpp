#include "cp.h"
#include "die.h"
#include "dce.h"
#include "inlining.h"
#include "unrolling.h"
#include "tail_recursion.h"


char mila::cp::Analysis::ID = 0;
char mila::cp::Optimization::ID = 0;
char mila::die::Optimization::ID = 0;
char mila::dce::Optimization::ID = 0;
char mila::inlining::Optimization::ID = 0;
char mila::unrolling::Optimization::ID = 0;
char mila::tailrecursion::Optimization::ID = 0;