#include <set>
#include <string>
#include <regex>
#include <stack>
#include <list>
#include <thread>
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

  typedef struct node
  {
    llvm::Instruction *ext_inst;
    std::string format = " ";
    std::string prased_decl = " ";
    std::list<std::string> param_list;
    int double_paramcount = 0;
  } extPrintfDetail;

  typedef struct state
  {
    bool flag = false;
    GlobalAlias *RDI;
    bool rdi_flag = false;
    GlobalAlias *RSI;
    bool rsi_flag = false;
    GlobalAlias *RDX;
    bool rdx_flag = false;
    GlobalAlias *RCX;
    bool rcx_flag = false;
    GlobalAlias *R8;
    bool r8_flag = false;
    GlobalAlias *R9;
    bool r9_flag = false;
    GlobalAlias *RAX;
    bool rax_flag = false;
    GlobalAlias *XMM0;
    bool xmm0_flag = false;
    GlobalAlias *XMM1;
    bool xmm1_flag = false;
    GlobalAlias *XMM2;
    bool xmm2_flag = false;
    GlobalAlias *XMM3;
    bool xmm3_flag = false;
    GlobalAlias *XMM4;
    bool xmm4_flag = false;
    GlobalAlias *XMM5;
    bool xmm5_flag = false;
    GlobalAlias *XMM6;
    bool xmm6_flag = false;
    GlobalAlias *XMM7;
    bool xmm7_flag = false;

    GlobalAlias *RSP;
    bool rsp_flag = false;

  } CurrentState;

  std::map<std::string, int> RegisterToNum_map = {
      {"RDI", 0},
      {"RSI", 1},
      {"RDX", 2},
      {"RCX", 3},
      {"R8", 4},
      {"R9", 5},
      {"XMM0", 6},
      {"XMM1", 7},
      {"XMM2", 8},
      {"XMM3", 9},
      {"XMM4", 10},
      {"XMM5", 11},
      {"XMM6", 12},
      {"XMM7", 13},
      {"RSP", 30},
      {"RAX", 31},
  };

  struct RdtscTransplanter : public ModulePass
  {
    static char ID;
    RdtscTransplanter() : ModulePass(ID) {}
    using BasicBlockListType = SymbolTableList<BasicBlock>;
    std::set<std::string> funcsToInst;
    llvm::CallInst *RdstcCallInst;
    llvm::GlobalVariable *gVar;
    // 存放最终识别到的需要修改的EEP函数
    std::list<llvm::Function *> EEPList;
    CurrentState State;
    bool runOnModule(Module &M)
    {
      LLVMContext &Context = M.getContext();
      llvm::ArrayType *arrType = llvm::ArrayType::get(llvm::Type::getInt64Ty(Context), 2);
      llvm::Constant *zeroInit = llvm::ConstantAggregateZero::get(arrType);
      gVar = new llvm::GlobalVariable(M, arrType, false,
                                      llvm::GlobalValue::PrivateLinkage, zeroInit, "gVarName");

      std::regex ext_pattern("ext_[0-9a-z]*_[0-9a-zA-Z]*");
      std::regex extPrintf_pattern("ext_[0-9a-z]*_[a-z]*printf");
      std::regex extScanf_pattern("ext_[0-9a-z]*_[a-z]*scanf");
      // 遍历所有的函数,找到有问题的函数
      for (Module::iterator mi = M.begin(); mi != M.end(); ++mi)
      {
        Function &f = *mi;
        if (std::regex_match(f.getName().str(), ext_pattern) )
        {
          std::size_t pos = f.getName().str().find_last_of('_');
          std::string result = f.getName().str().substr(pos + 1);
          
          errs() << f.getName() << "\n";
          if(Function *F = M.getFunction(result))
          {
            if (F->isVarArg()) {
                errs() << "Var Function: " << F->getName() << "\n";
                continue;
            }
            errs() << "Attributes before: \n";
            // Print function attributes
              const AttributeList &attrs = f.getAttributes();
            if (attrs.hasAttributes(AttributeList::FunctionIndex)) {
                for (Attribute attr : attrs.getAttributes(AttributeList::FunctionIndex)) {
                    errs() << "- " << attr.getAsString() << "\n";
                }
            }
            f.removeFnAttr(Attribute::NoInline);
            f.addFnAttr(Attribute::AlwaysInline);
            errs() << "Attributes after: \n";
            const AttributeList &attrs2 = f.getAttributes();
            for (Attribute attr : attrs2.getAttributes(AttributeList::FunctionIndex)) {
                    errs() << "- " << attr.getAsString() << "\n";
                }
          }

        }

      }




      return true;
    }



      // 接受一个代表寄存器的数字（1~6分别对应RDI~R9）和对应的调用指令
      // 解析什么函数被存入了该寄存器，将函数名存入全局EEPList中
      void parseRegisterAndPushEEPF(Module &M, int reg, llvm::CallInst *Inst) {
          std::string regName;
          std::regex registerPattern;
          std::regex rdi_pattern("RDI_[0-9a-z]*_[0-9a-z]*");
          std::regex rsi_pattern("RSI_[0-9a-z]*_[0-9a-z]*");
          std::regex rdx_pattern("RDX_[0-9a-z]*_[0-9a-z]*");
          std::regex rcx_pattern("RCX_[0-9a-z]*_[0-9a-z]*");
          std::regex r8_pattern("R8_[0-9a-z]*_[0-9a-z]*");
          std::regex r9_pattern("R9_[0-9a-z]*_[0-9a-z]*");
          std::regex rax_pattern("RAX_[0-9a-z]*_[0-9a-z]*");
          std::regex rbp_pattern("RBP_[0-9a-z]*_[0-9a-z]*");
          //先确定对应的pattern
          switch (reg) {
              case 1:
                registerPattern = rdi_pattern;
                break;
              case 2:
                registerPattern = rsi_pattern;
                break;
              case 3:
                registerPattern = rdx_pattern;
                break;
              case 4:
                registerPattern = rcx_pattern;
                break;
              case 5:
                registerPattern = r8_pattern;
                break;
              case 6:
                registerPattern = r9_pattern;
                break;
              default:
                break;
          }
          errs() << *Inst
                    << "\n";
          std::this_thread::sleep_for(std::chrono::seconds(1));
          //然后开始向前遍历，找到对应存入的函数名       
          BasicBlock *BB = Inst->getParent();
          bool FindFlag = false;
          while (BB != NULL && !FindFlag)
          {
            bool continue_flag = true;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            for (auto it = BB->rbegin(); it != BB->rend(); ++it) {
            
              // Break when we reach the given instruction
              if (&*it == Inst) {
                  continue_flag = false;
                }
              if (continue_flag == true) {
                continue;
              }
              //函数体
              // 首先找到store指令
              if (it->getOpcode() == Instruction::Store)
                {
                  //找到store指令
                  StoreInst *storeInst = dyn_cast<StoreInst>(&*it);
                  if (GlobalAlias *Subga = dyn_cast<GlobalAlias>(storeInst->getPointerOperand()))
                    {
                      if (std::regex_match(Subga->getName().str(), registerPattern))
                        {
                          errs() << "storeInst: " << *storeInst << "\n";
                          errs() << "Subga: " << Subga->getName() << "\n";
                          if (llvm::Function *EEPFunction = dyn_cast<llvm::Function>(storeInst->getOperand(0)))
                          { 
                            EEPList.push_back(EEPFunction);
                            FindFlag = true;
                            break;
                          }
                        }
                    }
                }

            //函数体
            }
            for (BasicBlock *Pred : predecessors(BB))
            {
              BB = Pred;
              break;
            }
            errs() << "BB的名字"
                    << "\n";
            errs() << BB->getName() << "\n";
          }

      }





      // Function to split the string by a delimiter and return an integer vector
      std::vector<int> splitStringToInt(const std::string &str, char delimiter) {
          std::vector<int> result;
          std::stringstream ss(str);
          std::string item;
          while (getline(ss, item, delimiter)) {
            result.push_back(std::stoi(item));
               
          }
          return result;
      }
  };
}

char RdtscTransplanter::ID = 0;

// 这边是对pass的注册
//  Register for opt
static RegisterPass<RdtscTransplanter> X("EXTInline", "External function inline");

