#include <set>
#include <string>
#include <regex>
#include <stack>
#include <list>
#include <map>
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
//这边写的是pass的具体功能
namespace {

  typedef struct node
{
  llvm::Instruction *ext_inst;
  std::string format=" ";
  std::string prased_decl=" ";
  std::list<std::string> param_list;
  int double_paramcount=0;
}extPrintfDetail;

  typedef struct state
{
    bool flag=false;
    GlobalAlias *RDI;
    bool rdi_flag=false;
    GlobalAlias *RSI;
    bool rsi_flag=false;
    GlobalAlias *RDX;
    bool rdx_flag=false;
    GlobalAlias *RCX;
    bool rcx_flag=false;
    GlobalAlias *R8;
    bool r8_flag=false;
    GlobalAlias *R9;
    bool r9_flag=false;
    GlobalAlias *RAX;
    bool rax_flag=false;
    GlobalAlias *XMM0;
    bool xmm0_flag=false;
    GlobalAlias *XMM1;
    bool xmm1_flag=false;
    GlobalAlias *XMM2;
    bool xmm2_flag=false;
    GlobalAlias *XMM3;
    bool xmm3_flag=false;
    GlobalAlias *XMM4;
    bool xmm4_flag=false;
    GlobalAlias *XMM5;
    bool xmm5_flag=false;
    GlobalAlias *XMM6;
    bool xmm6_flag=false;
    GlobalAlias *XMM7;
    bool xmm7_flag=false;

    GlobalAlias *RSP;
    bool rsp_flag=false;
    
    
}CurrentState;

  std::map<std::string,int> RegisterToNum_map ={
  {"RDI",0},
  {"RSI",1},
  {"RDX",2},
  {"RCX",3},
  {"R8",4},
  {"R9",5},
  {"XMM0",6},
  {"XMM1",7},
  {"XMM2",8},
  {"XMM3",9},
  {"XMM4",10},
  {"XMM5",11},
  {"XMM6",12},
  {"XMM7",13},
  {"RSP",30},
  {"RAX",31},
  };



  struct StatTransplanter : public ModulePass {
    static char ID;
    StatTransplanter() : ModulePass(ID) {} 
    using BasicBlockListType = SymbolTableList<BasicBlock>;
    std::set<std::string> funcsToInst;
    llvm::GlobalVariable* gVar;
    CurrentState State;
    
    StructType *StatArmType;
    StructType *StateX86_64Type;
    bool initStructArm=false;

    FunctionType *newType;
    Function *newFunc;
    bool initFunctionArm=false;

    Function *oldfunc;
    
    llvm::Function *printfFunc;
    bool runOnModule(Module &M) {
                llvm::LLVMContext &Context = M.getContext();

                std::string fxstatname="__fxstat";
                oldfunc = M.getFunction(fxstatname);
                bool find_flag=false;
                //遍历Module中的函数列表，找到想要处理的函数
                for (Module::iterator mi = M.begin(); mi != M.end(); ++mi) {
                    llvm::Function &f = *mi;
                    std::string fname=f.getName().str();
                    //不同函数的正则匹配规则
                    std::regex fxstat_pattern("__fxstat");

                    //待查找函数xstat
                    if (std::regex_match(fname,fxstat_pattern)) {
                      find_flag=true;
                        //遍历该函数的users
                        for (User *U : f.users()) {
                            if (Instruction *Inst = dyn_cast<Instruction>(U)) 
                            {
                                Instruction *Selection_Inst =Inst;
                                //先把State初始化一下
                                InailzeState(Selection_Inst);
                                
                                
                                DataMorphingAndSync(Inst,fxstatname);
                                errs() << "1" << "\n";
                                
                            }
                        }
                    }


                }
                if (!find_flag)
                {
                  errs() << "fstat use not find!" << "\n";
                    return true;
                }                
                errs() << "3" << "\n";
                //oldfunc->eraseFromParent();
                for (User *U : oldfunc->users()) {
                errs() << *U << "\n";
        
                }
                oldfunc->replaceAllUsesWith(newFunc);
                oldfunc->eraseFromParent();
                newFunc->setName(fxstatname);

                return true;
    }





  void DataMorphingAndSync(llvm::Instruction *callInst, std::string fname) {
  //当前版本还是先写死了对stat结构体的处理，等后续情况多了之后再弄个表做映射
  //这边是对%struct.stat*结构体的处理
  //%14 = call i32 @__fxstat(i32 %param0, i8* %10, %struct.stat* %12)
    Value *statPtr = callInst->getOperand(2); // Getting %12
    IRBuilder<> Builder(callInst);
    llvm::LLVMContext &Context = callInst->getContext();
    llvm::BasicBlock *fb = callInst->getParent();
    llvm::Function* func = fb->getParent();
    llvm::Module* M = func->getParent();
    StructType *TimespecType = M->getTypeByName("struct.timespec");


    // Define the structure `struct.stat_arm` using the existing `struct.timespec`
    std::vector<Type*> statArmMembers = {
        Type::getInt64Ty(Context), Type::getInt64Ty(Context),
        Type::getInt32Ty(Context), Type::getInt32Ty(Context),
        Type::getInt32Ty(Context), Type::getInt32Ty(Context),
        Type::getInt64Ty(Context), Type::getInt64Ty(Context),
        Type::getInt64Ty(Context), Type::getInt32Ty(Context),
        Type::getInt32Ty(Context), Type::getInt64Ty(Context),
        TimespecType, TimespecType, TimespecType,
        ArrayType::get(Type::getInt32Ty(Context), 2)
    };

    if(!initStructArm)
    {
      StatArmType = StructType::create(Context, statArmMembers, "struct.stat_arm");
      StateX86_64Type = M->getTypeByName("struct.stat");
      initStructArm=true;
    }
    // Allocate memory for one instance of the struct
    AllocaInst *statArmInstance = Builder.CreateAlloca(StatArmType, nullptr, "statArmInstance");

    //移动x86第一个i64参数,移到arm的第一个i64参数
    // Access first i64 of %struct.stat
    auto *firstElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 0, "firstElemPtr");
    auto *firstElementVal = Builder.CreateLoad(firstElementPtr, "firstElemVal");
    // Set the first i64 value of stat_arm
    auto *firstElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 0, "firstElemPtrArm");
    Builder.CreateStore(firstElementVal, firstElementPtrArm);
    
    //移动x86第二个i64参数,移到arm的第二个i64参数
    // Access second i64 of %struct.stat
    auto *secondElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 1, "secondElemPtr");
    auto *secondElementVal = Builder.CreateLoad(secondElementPtr, "secondElemVal");
    // Set the second i64 value of stat_arm
    auto *secondElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 1, "secondElemPtrArm");
    Builder.CreateStore(secondElementVal, secondElementPtrArm);

    //移动x86第三个i64参数,移到arm的第四个i32参数
    // Access third i64 of %struct.stat
    auto *thirdElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 2, "thirdElemPtr");
    auto *thirdElementVal = Builder.CreateLoad(thirdElementPtr, "thirdElemVal");
    //对i64进行截取
    auto *thirdTruncatedValue = Builder.CreateTrunc(thirdElementVal, Type::getInt32Ty(Builder.getContext()), "thirdTruncatedValue");
    // Set the forth i32 value of stat_arm
    auto *forthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 3, "forthElemPtrArm");
    Builder.CreateStore(thirdTruncatedValue, forthElementPtrArm);

    //移动x86第四个i32参数,移到arm的第三个i32参数
    // Access forth i32 of %struct.stat
    auto *forthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 3, "forthElemPtr");
    auto *forthElementVal = Builder.CreateLoad(forthElementPtr, "forthElemVal");
    // Set the third i32 value of stat_arm
    auto *thirdElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 2, "thirdElemPtrArm");
    Builder.CreateStore(forthElementVal, thirdElementPtrArm);

    //移动x86第五个i32参数,移到arm的第五个i32参数
    // Access fifth i32 of %struct.stat
    auto *fifthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 4, "fifthElemPtr");
    auto *fifthElementVal = Builder.CreateLoad(fifthElementPtr, "fifthElemVal");
    // Set the fifth i32 value of stat_arm
    auto *fifthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 4, "fifthElemPtrArm");
    Builder.CreateStore(fifthElementVal, fifthElementPtrArm);

    //移动x86第六个i32参数,移到arm的第六个i32参数
    // Access sixth i32 of %struct.stat
    auto *sixthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 5, "sixthElemPtr");
    auto *sixthElementVal = Builder.CreateLoad(sixthElementPtr, "sixthElemVal");
    // Set the sixth i32 value of stat_arm
    auto *sixthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 5, "sixthElemPtrArm");
    Builder.CreateStore(sixthElementVal, sixthElementPtrArm);

    //第七个参数是pad，不管

    //移动x86第八个i64参数,移到arm的第七个i64参数
    // Access eight i64 of %struct.stat
    auto *eightElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 7, "eightElemPtr");
    auto *eightElementVal = Builder.CreateLoad(eightElementPtr, "eightElemVal");
    // Set the seventh i64 value of stat_arm
    auto *seventhElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 6, "seventhElemPtrArm");
    Builder.CreateStore(eightElementVal, seventhElementPtrArm);

    //移动x86第九个i64参数,移到arm的第九个i64参数
    // Access ninth i64 of %struct.stat
    auto *ninthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 8, "ninthElemPtr");
    auto *ninthElementVal = Builder.CreateLoad(ninthElementPtr, "ninthElemVal");
    // Set the ninth i64 value of stat_arm
    auto *ninthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 8, "ninthElemPtrArm");
    Builder.CreateStore(ninthElementVal, ninthElementPtrArm);

    //移动x86第十个i64参数,移到arm的第十个i32参数
    // Access tenth i64 of %struct.stat
    auto *tenthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 9, "tenthElemPtr");
    auto *tenthElementVal = Builder.CreateLoad(tenthElementPtr, "tenthElemVal");
    // 对i64进行截取
    auto *tenthTruncatedValue = Builder.CreateTrunc(tenthElementVal, Type::getInt32Ty(Builder.getContext()), "tenthTruncatedValue");
    // Set the tenth i32 value of stat_arm
    auto *tenthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 9, "tenthElemPtrArm");
    Builder.CreateStore(tenthTruncatedValue, tenthElementPtrArm);

    //移动x86第十一个i64参数,移到arm的第十二个i64参数
    // Access eleventh i64 of %struct.stat
    auto *eleventhElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 10, "eleventhElemPtr");
    auto *eleventhElementVal = Builder.CreateLoad(eleventhElementPtr, "eleventhElemVal");
    // Set the twelfth i64 value of stat_arm
    auto *twelfthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 11, "twelfthElemPtrArm");
    Builder.CreateStore(eleventhElementVal, twelfthElementPtrArm);

    //移动x86第十二个%struct.timespec参数,移到arm的第十三个%struct.timespec参数
    // Access twelfth %struct.timespec of %struct.stat
    auto *twelfthTimespecPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 11, "twelfthTimespecPtr");
    auto *twelfthTimespecVal = Builder.CreateLoad(twelfthTimespecPtr, "twelfthTimespecVal");
    // Set the thirteenth %struct.timespec value of stat_arm
    auto *thirteenthTimespecPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 12, "thirteenthTimespecPtrArm");
    Builder.CreateStore(twelfthTimespecVal, thirteenthTimespecPtrArm);

    //移动x86第十三个%struct.timespec参数,移到arm的第十四个%struct.timespec参数
    // Access thirteenth %struct.timespec of %struct.stat
    auto *thirteenthTimespecPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 12, "thirteenthTimespecPtr");
    auto *thirteenthTimespecVal = Builder.CreateLoad(thirteenthTimespecPtr, "thirteenthTimespecVal");
    // Set the fourteenth %struct.timespec value of stat_arm
    auto *fourteenthTimespecPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 13, "fourteenthTimespecPtrArm");
    Builder.CreateStore(thirteenthTimespecVal, fourteenthTimespecPtrArm);

    //移动x86第十四个%struct.timespec参数,移到arm的第十五个%struct.timespec参数
    // Access fourteenth %struct.timespec of %struct.stat
    auto *fourteenthTimespecPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 13, "fourteenthTimespecPtr");
    auto *fourteenthTimespecVal = Builder.CreateLoad(fourteenthTimespecPtr, "fourteenthTimespecVal");
    // Set the fifteenth %struct.timespec value of stat_arm
    auto *fifteenthTimespecPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 14, "fifteenthTimespecPtrArm");
    Builder.CreateStore(fourteenthTimespecVal, fifteenthTimespecPtrArm);
    
    //最后的reverse部分就不同步了。

    if(!initFunctionArm)
    {
      errs() << "2" << "\n";
      //Function *oldfunc = M->getFunction(fname);

      // Get the original function type
      FunctionType *originalType = oldfunc->getFunctionType();

      // Create a new function type with the third parameter changed
      std::vector<Type*> params(originalType->params());


      params[2] = PointerType::getUnqual(StatArmType);  // Change the third parameter

      newType = FunctionType::get(originalType->getReturnType(), params, originalType->isVarArg());

      // Create a new function with the new type
      newFunc = llvm::Function::Create(newType, oldfunc->getLinkage(), "xstat_arm", M);
      initFunctionArm=true;
    }

    
    std::vector<llvm::Value *> asm_args;
    llvm::ConstantInt* constIntZero = llvm::ConstantInt::get(callInst->getContext(), llvm::APInt(32, 0, true));
    asm_args.push_back(constIntZero);
    //asm_args.push_back(callInst->getOperand(0));
    asm_args.push_back(callInst->getOperand(1));
    asm_args.push_back(statArmInstance);
    auto newcall =Builder.CreateCall(newFunc, asm_args);
    //callInst->setOperand(2, statArmInstance);

    
    errs() << *callInst << "\n";
    for (User *U : callInst->users()) {
      errs() << *U << "666\n";
      if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        Inst->setOperand(0, newcall);
        errs() << *Inst << "666\n";
        errs() << *newcall << "666\n";
      }
    }
    errs() << "7777777777" << "\n";

    
    
/*    
    llvm::CallInst *callInst2 = dyn_cast<llvm::CallInst>(callInst);
      Function *calledFunction = callInst2->getCalledFunction();
      //calledFunction->setOperand(2, statArmInstance);
      calledFunction->replaceAllUsesWith(newFunc);
      calledFunction->eraseFromParent();
      newFunc->setName(fname);
      
      for (User *U : newFunc->users()) {
        errs() << *U << "\n";
        
      }
*/



    //返回后再把stat_arm中的内容全部同步回stat_x86

    //移动arm第一个i64参数,移到x86的第一个i64参数
    // Access first i64 of %struct.stat_arm
    firstElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 0, "firstElemPtrArm");
    auto *firstElementValArm = Builder.CreateLoad(firstElementPtrArm, "firstElemValArm");
    // Set the first i64 value of stat_x86
    firstElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 0, "firstElemPtr");
    Builder.CreateStore(firstElementValArm, firstElementPtr);

    //移动arm第二个i64参数,移到x86的第二个i64参数
    // Access second i64 of %struct.stat_arm
    secondElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 1, "secondElemPtrArm");
    auto *secondElementValArm = Builder.CreateLoad(secondElementPtrArm, "secondElemValArm");
    // Set the second i64 value of stat_x86
    secondElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 1, "secondElemPtr");
    Builder.CreateStore(secondElementValArm, secondElementPtr);

    //移动arm第三个i32参数,移到x86的第四个i32参数
    // Access third i32 of %struct.stat_arm
    thirdElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 2, "thirdElemPtrArm");
    auto *thirdElementValArm = Builder.CreateLoad(thirdElementPtrArm, "thirdElemValArm");
    // Set the forth i32 value of stat_x86
    forthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 3, "forthElemPtr");
    Builder.CreateStore(thirdElementValArm, forthElementPtr);

    //移动arm第四个i32参数,移到x86的第三个i64参数
    // Access forth i32 of %struct.stat_arm
    forthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 3, "forthElemPtrArm");
    auto *forthElementValArm = Builder.CreateLoad(forthElementPtrArm, "forthElemValArm");
    //将i32拓展成i64
    auto *extendedValue = Builder.CreateSExt(forthElementValArm, Type::getInt64Ty(Builder.getContext()), "extendedValue");
    // Set the third i64 value of stat_x86
    thirdElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 2, "thirdElemPtr");
    Builder.CreateStore(extendedValue, thirdElementPtr);

    //移动arm第五个i32参数,移到x86的第五个i32参数
    // Access fifth i32 of %struct.stat_arm
    fifthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 4, "fifthElemPtrArm");
    auto *fifthElementValArm = Builder.CreateLoad(fifthElementPtrArm, "fifthElemValArm");
    // Set the fifth i32 value of stat_x86
    fifthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 4, "fifthElemPtr");
    Builder.CreateStore(fifthElementValArm, fifthElementPtr);

    //移动arm第六个i32参数,移到x86的第六个i32参数
    // Access sixth i32 of %struct.stat_arm
    sixthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 5, "sixthElemPtrArm");
    auto *sixthElementValArm = Builder.CreateLoad(sixthElementPtrArm, "sixthElemValArm");
    // Set the sixth i32 value of stat_x86
    sixthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 5, "sixthElemPtr");
    Builder.CreateStore(sixthElementValArm, sixthElementPtr);

    //移动arm第七个i64参数,移到x86的第八个i64参数
    // Access seventh i64 of %struct.stat_arm
    seventhElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 6, "seventhElemPtrArm");
    auto *seventhElementValArm = Builder.CreateLoad(seventhElementPtrArm, "seventhElemValArm");
    // Set the eighth i64 value of stat_x86
    eightElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 7, "eightElemPtr");
    Builder.CreateStore(seventhElementValArm, eightElementPtr);

    //arm的第8个参数是padding，不管

    //移动arm第九个i64参数,移到x86的第九个i64参数
    // Access ninth i64 of %struct.stat_arm
    ninthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 8, "ninthElemPtrArm");
    auto *ninthElementValArm = Builder.CreateLoad(ninthElementPtrArm, "ninthElemValArm");
    // Set the ninth i64 value of stat_x86
    ninthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 8, "ninthElemPtr");
    Builder.CreateStore(ninthElementValArm, ninthElementPtr);

    //移动arm第十个i32参数,移到x86的第十个i64参数
    // Access tenth i32 of %struct.stat_arm
    tenthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 9, "tenthElemPtrArm");
    auto *tenthElementValArm = Builder.CreateLoad(tenthElementPtrArm, "tenthElemValArm");
    //将i32拓展成i64
    auto *extendedValueArm = Builder.CreateSExt(tenthElementValArm, Type::getInt64Ty(Builder.getContext()), "extendedValueArm");
    // Set the tenth i64 value of stat_x86
    tenthElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 9, "tenthElemPtr");
    Builder.CreateStore(extendedValueArm, tenthElementPtr);

    //arm的第十一个参数是padding，不管

    //移动arm第十二个i64参数,移到x86的第十一个i64参数
    // Access twelfth i64 of %struct.stat_arm
    twelfthElementPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 11, "twelfthElemPtrArm");
    auto *twelfthElementValArm = Builder.CreateLoad(twelfthElementPtrArm, "twelfthElemValArm");
    // Set the eleventh i64 value of stat_x86
    eleventhElementPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 10, "eleventhElemPtr");
    Builder.CreateStore(twelfthElementValArm, eleventhElementPtr);

    //移动arm第十三个%struct.timespec参数,移到x86的第十二个%struct.timespec参数
    // Access thirteenth %struct.timespec of %struct.stat_arm
    thirteenthTimespecPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 12, "thirteenthTimespecPtrArm");
    auto *thirteenthTimespecValArm = Builder.CreateLoad(thirteenthTimespecPtrArm, "thirteenthTimespecValArm");
    // Set the twelfth %struct.timespec value of stat_x86
    twelfthTimespecPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 11, "twelfthTimespecPtr");
    Builder.CreateStore(thirteenthTimespecValArm, twelfthTimespecPtr);

    //移动arm第十四个%struct.timespec参数,移到x86的第十三个%struct.timespec参数
    // Access fourteenth %struct.timespec of %struct.stat_arm
    fourteenthTimespecPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 13, "fourteenthTimespecPtrArm");
    auto *fourteenthTimespecValArm = Builder.CreateLoad(fourteenthTimespecPtrArm, "fourteenthTimespecValArm");
    // Set the thirteenth %struct.timespec value of stat_x86
    thirteenthTimespecPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 12, "thirteenthTimespecPtr");
    Builder.CreateStore(fourteenthTimespecValArm, thirteenthTimespecPtr);

    //移动arm第十五个%struct.timespec参数,移到x86的第十四个%struct.timespec参数
    // Access fifteenth %struct.timespec of %struct.stat_arm
    fifteenthTimespecPtrArm = Builder.CreateStructGEP(StatArmType, statArmInstance, 14, "fifteenthTimespecPtrArm");
    auto *fifteenthTimespecValArm = Builder.CreateLoad(fifteenthTimespecPtrArm, "fifteenthTimespecValArm");
    // Set the fourteenth %struct.timespec value of stat_x86
    fourteenthTimespecPtr = Builder.CreateStructGEP(StateX86_64Type, statPtr, 13, "fourteenthTimespecPtr");
    Builder.CreateStore(fifteenthTimespecValArm, fourteenthTimespecPtr);

    //后面的是预留的部分，不管

      callInst->eraseFromParent();









    
    
  }

 

  


  void InailzeState(llvm::Instruction *callInst) {
    //维护了一个表示当前全局寄存器状态的结构体State。
    //先判断是否已经初始化，只需初始化一次。
    if(State.flag==false)
    {
      //遍历所有的GlobalAlias，找到我们想要的。
      for (GlobalAlias &GA : callInst->getModule()->aliases()) { 
        GlobalAlias *GLa = dyn_cast<GlobalAlias>(&GA);
        llvm::StringRef GLaName=GLa->getName();
        std::string glaName=GLaName.str();
        //errs() << glaName<< "\n";
        //mcsema翻译后的ir中的寄存器变量格式为：@RDI_2296_1c91e98
        //这边通过正则来确定该变量指代的是哪个寄存器
        std::size_t x=glaName.find_first_of("_");
        glaName=glaName.substr(0,x);
        //map可能出现匹配不到的情况，这边先count一下
        if(!RegisterToNum_map.count(glaName))
        {
          continue;  
        }
        int num=RegisterToNum_map[glaName];

        //RSP做特殊处理，我们需要的是一个指向i64的IntegerType的@RSP
        if(num==30)
        {
          if(GLa->getType()->getPointerElementType()->isIntegerTy())
          {
            llvm::Type *tt=GLa->getType()->getPointerElementType();
            llvm::IntegerType *intTT = dyn_cast<llvm::IntegerType>(tt);
            if(intTT->getBitWidth()==64)
            {
                State.RSP=GLa;
                errs() << *GLa->getType()<< "\n";
                State.rsp_flag=true;
            }
          }

        }
        //RAX也特殊处理，我们需要的是一个指向i64的IntegerType的@RAX
        if(num==31)
        {
          if(GLa->getType()->getPointerElementType()->isIntegerTy())
          {
            llvm::Type *tt=GLa->getType()->getPointerElementType();
            llvm::IntegerType *intTT = dyn_cast<llvm::IntegerType>(tt);
            if(intTT->getBitWidth()==64)
            {
                State.RAX=GLa;
                State.rax_flag=true;
            }
          }

        }

        if(num>=0 && num <=13)
        {
          //RSI~R8在此处理
          if(GLa->getType()->getPointerElementType()->isIntegerTy())
          {
            //errs() << GLa->getName()<< "\n";
            llvm::Type *tt=GLa->getType()->getPointerElementType();
            llvm::IntegerType *intTT = dyn_cast<llvm::IntegerType>(tt);
            if(intTT->getBitWidth()==64)
            {
              switch (num)
              {
              case 0:
                State.RDI=GLa;
                State.rdi_flag=true;
                break;
              case 1:
                State.RSI=GLa;
                State.rsi_flag=true;
                break;
              case 2:
                State.RDX=GLa;
                State.rdx_flag=true;
                break;
              case 3:
                State.RCX=GLa;
                State.rcx_flag=true;
                break;
              case 4:
                State.R8=GLa;
                State.r8_flag=true;
                break;
              case 5:
                State.R9=GLa;
                State.r9_flag=true;
                break;  
              
              default:
                break;
              }
            }
          }
          //XMM0~XMM7在此处理
          else if (GLa->getType()->getPointerElementType()->isDoubleTy())
          {
            llvm::Type *tt=GLa->getType()->getPointerElementType();
            std::string Name=GLaName.str();
            //因为XMM0基础存器在mcsema的表示中可能存在如下情况：
            //@XMM0_16_1c91da8 = private thread_local(initialexec) alias double,
            //@XMM0_24_1c91da8 = private thread_local(initialexec) alias double,
            //上述两个GLA都是double类型，无法直接通过llvm type判断，
            //区别在函数名中的“16”和“24”所表示的寄存器位数中，我们需要的是16位的，故这边再做一层正则匹配。
            Name=Name.substr(x+1,sizeof(Name));
            std::size_t x2=Name.find_first_of("_");
            std::string width= GLaName.str().substr(x+1,x2);
            int width_int = std::stoi(width);

            // Check the condition
            //这边是新的解决上面的16，24的方法。深入调研发现，如果中间的数字-16能够被64整除，则取的是XMM*寄存器的前64位，而double类型这前64位就够用了。
            bool continue_flag=false;
            if ((width_int - 16) % 64 == 0) {
              continue_flag=true;
            } 
            else {
              continue;

            }

            if(continue_flag==true)
            {
              switch (num)
              {
              case 6:
                State.XMM0=GLa;
                State.xmm0_flag=true;
                break;
              case 7:
                State.XMM1=GLa;
                State.xmm1_flag=true;
                break;
              case 8:
                State.XMM2=GLa;
                State.xmm2_flag=true;
                break;
              case 9:
                State.XMM3=GLa;
                State.xmm3_flag=true;
                break;
              case 10:
                State.XMM4=GLa;
                State.xmm4_flag=true;
                break;
              case 11:
                State.XMM5=GLa;
                State.xmm5_flag=true;
                break;
              case 12:
                State.XMM6=GLa;
                State.xmm6_flag=true;
                break;
              case 13:
                State.XMM7=GLa;
                State.xmm7_flag=true;
                break;    
              
              default:
                break;
              }
            }

          }else{
            continue;
          }
          
            
        
        }

        
      }
      State.flag=true;
    }

    
  }




  };
}


char StatTransplanter::ID = 0;

//这边是对pass的注册
// Register for opt
static RegisterPass<StatTransplanter> X("fstat", "fstat sync Pass");

// Register for clang
/*static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
  [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
    PM.add(new Hello());
  });*/