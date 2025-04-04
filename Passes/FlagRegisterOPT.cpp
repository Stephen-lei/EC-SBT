#include <set>
#include <string>
#include <map>
#include <list>
#include <iostream>
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
#include "llvm/IR/SymbolTableListTraits.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

// 定义标志位寄存器的映射
std::map<std::string, int> RegisterToNum_map = {
    {"AF", 31},
    {"CF", 32},
    {"OF", 33},
    {"PF", 34},
    {"SF", 35},
    {"ZF", 36},
};

struct RdtscTransplanter : public ModulePass {
    static char ID;
    RdtscTransplanter() : ModulePass(ID) {}

    bool runOnModule(Module &M) {
        // 遍历所有的GlobalAlias，找到我们想要的
        for (GlobalAlias &GA : M.aliases()) {
            std::string glaName = GA.getName().str();

            // 通过下划线切割全名并获取寄存器前缀
            std::size_t x = glaName.find_first_of("_");
            if (x != std::string::npos) {
                glaName = glaName.substr(0, x);  // 获取前缀部分
            }

            // 如果该前缀是标志位寄存器名，则删除所有相关的Store指令
            if (RegisterToNum_map.count(glaName)) {
                // 遍历全局别名的所有使用
                bool hasLoadInst = false;
                //遍历两遍，第一遍遍历包不包含load指令，如果包含，则直接跳过该标志位寄存器
                for (User *usedValue : GA.users())
                {
                    // 检查是否是StoreInst指令
                    if (auto *loadInst = dyn_cast<LoadInst>(usedValue)) {
                            errs() << "存在loadInst: " << *loadInst << "\n";
                            errs() << "不处理该标志位寄存器 \n";
                            hasLoadInst = true;
                            break;
                        }
                }
                if (hasLoadInst) {
                    continue;
                }
                //第二遍开始删除store指令
                for (User *usedValue : GA.users())
                {
                    //Value *usedValue = it->get();
                    errs() << "usedValue: " << *usedValue << "\n";
                    // 检查是否是StoreInst指令
                    if (auto *storeInst = dyn_cast<StoreInst>(usedValue)) {
                            errs() << "准备删除storeInst: " << *storeInst << "\n";
                            storeInst->eraseFromParent();
                        }
                    
                }
            }
        }

        return true;
    }
};

char RdtscTransplanter::ID = 0;

// 注册Pass，名称为 FlagOPT
static RegisterPass<RdtscTransplanter> X("FlagOPT", "A pass to optimize the redundant flag register store instructions");
