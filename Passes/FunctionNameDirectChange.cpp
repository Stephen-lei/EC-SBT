#include <set>
#include <string>
#include <regex>
#include <stack>
#include <list>
#include <map>
#include <iostream>
#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/StringMapEntry.h"
#include "llvm/IR/SymbolTableListTraits.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/User.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;
// 这边写的是pass的具体功能
namespace
{
  std::map<std::string, std::string> FunctionToBeChangedMap = {
      {"sqrtf__original", "sqrtf"},
      {"sqrt__original", "sqrt"},
  };

  struct FunctionNmaeDirectChange : public ModulePass
  {
    static char ID;
    FunctionNmaeDirectChange() : ModulePass(ID) {}
    using BasicBlockListType = SymbolTableList<BasicBlock>;
    std::set<std::string> funcsToInst;
    llvm::CallInst *RdstcCallInst;
    llvm::GlobalVariable *gVar;
    bool runOnModule(Module &M)
    {
      LLVMContext &Context = M.getContext();
      //根据map表查找IR中是否存在该函数，存在则替换名字
      for (const auto &renamePair : FunctionToBeChangedMap) {
      // Attempt to find the function in the module by its old name
      Function *func = M.getFunction(renamePair.first);
      
      // If the function is found, rename it
      if (func != nullptr) {
          llvm::Function *oldFunc = M.getFunction(renamePair.second);
          //需要再进一步判断rename后的function在IR中是否已经存在
          if (!oldFunc) {
            //如果不存在，则直接修改函数名
            func->setName(renamePair.second);
            llvm::errs() << "Function " << renamePair.first << " directly renamed to: " << renamePair.second << "\n";
          }else{
            //如果存在，则不能直接修改，而是把原来的函数的user替换为新的函数
            oldFunc->replaceAllUsesWith(func);
            oldFunc->eraseFromParent();
            func->setName(renamePair.second);
            llvm::errs() << "Function " << renamePair.first << " first changed the pre uses and renamed to: " << renamePair.second << "\n";
          }          
      } else {
          llvm::errs() << "Function " << renamePair.first << " not found.\n";
      }
    }


      return true;
    }


  };
}

char FunctionNmaeDirectChange::ID = 0;

// 这边是对pass的注册
//  Register for opt
static RegisterPass<FunctionNmaeDirectChange> X("funcNC", "A pass directly change all the function name that is in the map.");

