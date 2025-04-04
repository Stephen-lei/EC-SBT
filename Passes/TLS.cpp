#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

#define DEBUG_TYPE "tls"

namespace {

std::set<StringRef> EEPoints = {
    "__sysv_signal", "sysv_signal", "signal", "ssignal", "sigset",
    "atexit", "at_quick_exit", "on_exit", "bsearch", "qsort", "qsort_r",
    "clone", "pthread_create", "pthread_once", "pthread_key_create",
    "pthread_atfork", "scandir", "scandir64", "scandirat", "scandirat64",
    "_obstack_begin", "_obstack_begin_1", "glob", "glob64",
};

std::set<StringRef> TLSPrefix = {
    "RBX","RAX","RCX","RDX","RSP","RBP","RSI","RDI","RIP",
    "R8","R9","R10","R11","R12","R13","R14","R15",
    "CF","PF","ZF","SF","OF","AF",
    // "TF","IF",
    // "XMM0","XMM1","XMM2",
    // "XMM3","XMM4","XMM5","XMM6","XMM7","XMM8","XMM9",
};

class XMMReg {
public:
    XMMReg(StringRef XMMReg) {
        Basename = XMMReg.substr(0,XMMReg.find("_")).str();
        StringRef rest = XMMReg.substr(XMMReg.find("_")+1);
        offset = std::stoi(rest.substr(0,rest.find("_")).str());
        // errs()<<"XMMReg init: Name : "<<XMMReg<<" Basename : "<<Basename<<" offset : "<<offset<<" withOff : "<<get_NameWithOff()<<"\n";
    }

    static bool isXMMReg(StringRef Reg) {
        if(Reg.contains("Local_")) {
            Reg = Reg.substr(6);
        }
        return Reg.substr(0,3) == "XMM";
    }

    StringRef get_Basename() {
        return Basename;
    }
    //获取相对于__mcsema_reg_state的字节偏移量
    int get_offset() {
        int ind = Basename.back() - '0';
        return 16 + ind * 64;
    }
    //获取相对于一个Local变量的字节偏移量
    int get_local_offset() {
        return offset - get_offset();
    }
    //形如XMM0_16的样子
    std::string get_NameWithOff() {
        // return std::string(Basename)+"_";
        return std::string(Basename)+"_"+std::to_string(offset);
    }
    
private:
    std::string Basename;
    int offset;//这个是相对于__mcsema_reg_state的偏移量
};

class TLSImpl {
public:
    TLSImpl(Function &F,FunctionAnalysisManager &AM):
        F(F),AM(AM),
        LI(AM.getResult<LoopAnalysis>(F)),
        DT(AM.getResult<DominatorTreeAnalysis>(F)),
        SE(AM.getResult<ScalarEvolutionAnalysis>(F)),
        Builder(&*F.getEntryBlock().getFirstInsertionPt()) {};
    bool ShouldHandle();
    bool Localize_TLS_Loop();
    bool Localize_TLS_BB();

private:
#if 0
    //原来的函数，现在没有用到
    bool ShouldHandleLoop(Loop *L);
#endif
    bool SkipFunction(const StringRef &Fname);
//对于loop的处理
    int GetInfo(Loop *L,std::set<Value*> &TLSRegs,std::set<Instruction*> &SkipFunctions,int &UseTLSRegs);
    bool ShouldHandleLoop(Loop *L,std::set<Value*> &TLSRegs,std::set<Instruction*> &SkipFunctions,int Insts,int UseTLSRegs);
    void GetTLSRegs(Loop *L, std::set<Value*> &TLSRegs);
    bool runOnLoop(Loop *L);
//对于非loop中的BB的处理
    int GetInfo(BasicBlock *BB,std::set<Value*> &TLSRegs,std::set<Instruction*> &SkipFunctions,int &UseTLSRegs);
    bool ShouldHandleBB(BasicBlock *BB,std::set<Value*> &TLSRegs,std::set<Instruction*> &SkipFunctions,int Insts,int UseTLSRegs);
    bool runOnBB(BasicBlock *BB);
//
private:
    Function &F;
    FunctionAnalysisManager &AM;
    LoopInfo &LI;
    DominatorTree &DT;
    ScalarEvolution &SE;
    IRBuilder<> Builder;
    std::set<BasicBlock*> skipBB;
};

bool TLSImpl::SkipFunction(const StringRef &Fname) {
    bool skip = false;
    {
        StringRef subname = Fname.substr(0,4);
        if(subname == "ext_" || subname == "sub_") {
            skip |= true;
        }
    }
    if(Fname == "__mcsema_init_reg_state") {
        skip |= true;
    }
    {
        if(EEPoints.count(Fname)) {
            skip |= true;
        }
    }
    return skip;
}

//跳过有些不需要处理的函数
bool TLSImpl::ShouldHandle() {
    StringRef name = F.getName();
    std::set<StringRef> tar = {
        // "sub_403990_intrapred_luma_16x16",

        // "sub_459870_SetupLargerBlocks",
        // "sub_45a720_SetupFastFullPelSearch",
        // "sub_45da80_FastFullPelBlockMotionSearch",

        // "sub_46cfd0_BlockMotionSearch",
        // "sub_471110_PartitionMotionSearch",

        // "sub_48f9c0_encode_one_macroblock",
        // "sub_49f440_encode_one_slice",
        // "sub_427230_code_a_picture",
        // "sub_429050_frame_picture",
        // "sub_427520_encode_one_frame",
        // "sub_431d50_main",
        // "sub_49be30_FastPelY_14",
        "sub_42a2f0_UnifiedOneForthPix",

    };
    // if(tar.count(name)) return true; else return false;
    // if(SkipFunction(name)) {
    //     return true;
    // }
    if(name.substr(0,4) == "sub_") {
        return true;
    }
    return false;
}
#if 0 //这一部分暂时都不要了
//检查是否需要进行TLS本地化，如果其中有一些特定的callee，那就无法进行
bool TLSImpl::ShouldHandleLoop(Loop *L) {
    bool result = true;
    int skipfunction_count = 0;
    for(auto *BB:L->getBlocks()) {
        for(auto &Inst : make_early_inc_range(*BB)) {
            // errs()<<Inst.getName()<<"\n";
            if(CallInst *CI = dyn_cast<CallInst>(&Inst)) {
                Function *Callee = CI->getCalledFunction();
                if(Callee && Callee->hasName()) {
                    // errs()<<"callee in loop : "<<Callee->getName()<<"\n";
                    if(SkipFunction(Callee->getName())) {
                        result = false;
                        skipfunction_count ++;
                    }
                } 
            }
        }
    }
    return result;
}

void TLSImpl::GetTLSRegs(Loop *L,std::set<Value*> &TLSRegs) {
    for(auto *BB:L->getBlocks()) {
        for(auto &Inst:*BB) {
            if(!isa<StoreInst>(Inst) && !isa<LoadInst>(Inst) && !isa<PHINode>(Inst)) {
                continue;
            }
            for(Use &U:Inst.operands()) {
                Value *v = U.get();
                if(!v->hasName()) continue;
                StringRef name = v->getName();
                // errs()<<name<<"\n";
                if(TLSPrefix.count(name.substr(0,name.find("_",0)))) {
                    TLSRegs.insert(v);
                }
            }
        }
    }
}
#endif

//获取TLSRegs和SkipFunctions集合，并且计算循环包含的Instruction数量，为是否处理循环做准备
int TLSImpl::GetInfo(Loop *L,std::set<Value*> &TLSRegs,std::set<Instruction*> &SkipFunctions,int &UseTLSRegs) {
    int Insts = 0;
    for(auto *BB:L->getBlocks()) {
        Insts += (*BB).size();
        for(auto &Inst:*BB) {
            if(isa<LoadInst>(Inst) || isa<StoreInst>(Inst) || isa<PHINode>(Inst)) {
                for(Use &U:Inst.operands()) {
                    Value *V = U.get();
                    if(!V->hasName() || V->getName().size() <= 3) continue;
                    StringRef name = V->getName();
                    if(TLSPrefix.count(name.substr(0,name.find("_",0)))) {
                       TLSRegs.insert(V);
                       UseTLSRegs ++;
                       if(XMMReg::isXMMReg(name)) {
                            errs()<<F.getName()<<" "<<name<<"\n";
                       }
                    }
                }
            }
            if(CallInst *CI = dyn_cast<CallInst>(&Inst)) {
                Function *Callee = CI->getCalledFunction();
                if(Callee && Callee->hasName()) {
                    if(SkipFunction(Callee->getName()) || !Callee->isIntrinsic()){
                        SkipFunctions.insert(CI);
                    }
                } else {
                    SkipFunctions.insert(CI);
                }
            }
        }
    }
    return Insts;
}

bool TLSImpl::ShouldHandleLoop(Loop *L,std::set<Value*> &TLSRegs,
        std::set<Instruction*> &SkipFunctions,int Insts,int UseTLSRegs) {
    std::set<StringRef> st;
    for(auto V:TLSRegs) {
        st.insert(V->getName().substr(0,V->getName().find("_")));
    }
    errs()<<"Loop Info: RegNum:"<<st.size()<<" SkipFunctionNum:"<<SkipFunctions.size()<<" UseReg:"<<UseTLSRegs<<" LoopInsts:"<<Insts<<"\n";
    //如果TLSRegs为空，则没有可以进行替换的
    if(TLSRegs.empty()) 
        return false;
    //变量在循环中使用过少,两次都没有用到
    // if(st.size() * 2 > UseTLSRegs) {
    //     errs()<<"too few Use TLSRegs\n";
    //     return false;
    // }
    //如果没有skipfunction，那么可以handle
    if(SkipFunctions.empty()) {
        //如果使用TLSReg的数量相比于循环大小过小
        // if(UseTLSRegs * 150 < Insts) {
        //     errs()<<"too few TLSRegs\n";    
        //     return false;
        // }
        return true;
    }
    return true;
    if(st.size() * SkipFunctions.size() * 4 < UseTLSRegs) {
        errs()<<"handle loop with skipfunctions\n";
        return true;
    }
    return false;
}


bool TLSImpl::runOnLoop(Loop *L) {
    // L->dump();
    bool Changed = false;

    std::set<Value*> TLSRegs;
    std::set<Instruction *> SkipFunctions;
    int UseTLSRegs = 0;
    std::unordered_map<Value*,int> UseTLS;
    int Insts = GetInfo(L,TLSRegs,SkipFunctions,UseTLSRegs);
    if(!ShouldHandleLoop(L,TLSRegs,SkipFunctions,Insts,UseTLSRegs)) {
        for(Loop *SubLoop : L->getSubLoops()) {
            Changed |= runOnLoop(SubLoop);
        }
        return Changed;        
    }
    errs()<<"Handle Loop:"<<L->getName()<<"\n";

    // if(!ShouldHandleLoop(L)) {
    //     for(Loop *SubLoop : L->getSubLoops()) {
    //         Changed |= runOnLoop(SubLoop);
    //     }
    //     return Changed;
    // }
    // GetTLSRegs(L,TLSRegs);  //获取循环中需要优化的目标TLS寄存器
    errs()<<"Total TLSRegs : "<<TLSRegs.size()<<"\n";
    auto Preheader = L->getLoopPreheader();
    skipBB.insert(Preheader);
    // IRBuilder<> Builder(&*Preheader->begin());

    StringMap<Value*> map,map2;//map维护本地局部变量名字到Alloca的映射，map2维护本地局部变量名字到TLSReg本身的映射
    for(auto TLSReg:TLSRegs) {
        Builder.SetInsertPoint(&Preheader->back());
        StringRef Name = TLSReg->getName();
        if(XMMReg::isXMMReg(Name)) {
            std::string Local = (std::string(std::string("Local_")+XMMReg(Name).get_NameWithOff()));//XMM寄存器中分段的地址
            StringRef LocalBase = "Local_"+XMMReg(Name).get_Basename().str();//XMM寄存器的首地址
            // errs()<<"RegName : "<<Name<<" Local : "<<Local<<" LocalBase : "<<LocalBase<<"\n";
            Value *Local_ptr;
            if(map.count(Local)) {
                Local_ptr = map[Local];
            } else {
                Value *AI_Base;//局部XMM寄存器alloca的值
                if(map.count(LocalBase)) {
                    AI_Base = map[LocalBase];
                } else {
                    auto Ty128 = Type::getInt128Ty(F.getContext());
                    auto Ty64 = Type::getInt64Ty(F.getContext());
                    auto Ty8 = Type::getInt8Ty(F.getContext());
                    // AI_Base = Builder.CreateAlloca(Ty128,nullptr,LocalBase);
                    AI_Base = Builder.CreateAlloca(Builder.getInt32Ty(),Builder.getInt32(4),LocalBase);
                    // AI_Base = Builder.CreateAlloca(Ty8,ConstantInt::get(Ty64,16),LocalBase);
                    map.insert({LocalBase,AI_Base});
                    Value *regs_base = F.getParent()->getGlobalVariable("__mcsema_reg_state",true);
                    Value *reg_begin = Builder.CreateGEP(Ty8,regs_base,ConstantInt::get(Ty64,XMMReg(Name).get_offset()));
                    map2.insert({LocalBase,reg_begin});
                    LoadInst *LI = Builder.CreateLoad(Ty128,reg_begin,LocalBase);
                    Builder.CreateStore(LI,AI_Base);
                }
                auto Ty8 = Type::getInt8Ty(F.getContext());
                auto Ty64 = Type::getInt64Ty(F.getContext());
                // Value *temp = Builder.CreatePtrToInt(AI_Base,Builder.getIntPtrTy(F.getDataLayout()));
                // temp = Builder.CreateAdd(temp,ConstantInt::get(Ty64,XMMReg(Name).get_local_offset()));
                // Local_ptr = Builder.CreateIntToPtr(temp,Builder.getPtrTy(),Local);
                Local_ptr = Builder.CreateGEP(Ty8,AI_Base,ConstantInt::get(Ty64,XMMReg(Name).get_local_offset()),Local);
                map.insert({Local,Local_ptr});
            }
            TLSReg->replaceUsesWithIf(Local_ptr,[&](Use &U) {
                User *user = U.getUser();
                if(Instruction *Inst = dyn_cast<Instruction>(user)) {
                    if(L->contains(Inst)) {
                        return true;
                    }
                }
                return false;
            });
            continue;
        }
        StringRef Local = "Local_"+std::string(Name.substr(0,Name.find("_")));
        Value *AI;
        if(map.count(Local)) {
            AI = map[Local];
        } else {
            errs()<<"Loop Preheader : "<<Preheader->getName()<<"\n";
            IntegerType *IT;
            if(Local.back() == 'F') {
                IT = Type::getInt8Ty(F.getContext());
            } else {
                IT = Type::getInt64Ty(F.getContext());
            }
            AI = Builder.CreateAlloca(IT,nullptr,Local);
            // AI = Builder.CreateAlloca(Type::getInt8Ty(F.getContext()),ConstantInt::get(Type::getInt8Ty(F.getContext()),8),Local);
            map.insert({Local,AI});
            map2.insert({Local,TLSReg});
            LoadInst *LI = Builder.CreateLoad(IT,TLSReg);
            StoreInst *SI = Builder.CreateStore(LI,AI);
        }
        // errs()<<TLSReg->getNumUses()<<" "<<"\n";
        TLSReg->replaceUsesWithIf(AI,[&](Use &U){
            User *user = U.getUser();
            if(Instruction *Inst = dyn_cast<Instruction>(user)) {
                if(L->contains(Inst)) {
                    // errs() << "inLoop whether dominate : " << AI->getName() <<" "<<Inst->getParent()->getName() <<"\n";
                    return true;
                }
            }
            return false;
        });
    }

    SmallVector<BasicBlock*> ExitBlocks;
    L->getExitBlocks(ExitBlocks);
    for(auto BB:ExitBlocks) {
        Builder.SetInsertPoint(BB->getFirstInsertionPt());
        errs()<<"Exit Bloks : "<<BB->getName()<<"\n";
        skipBB.insert(BB);
        for(auto it=map.begin();it!=map.end();it++){
            auto AI = it->second;
            if(!map2.count(it->first())) continue;
            auto TLSReg = map2[it->first()];
            IntegerType *IT;
            if(XMMReg::isXMMReg(it->first())) {
                IT = Type::getInt128Ty(F.getContext());
            } else if(it->first().back() == 'F') {
                IT = Type::getInt8Ty(F.getContext());
            } else {
                IT = Type::getInt64Ty(F.getContext());
            }
            LoadInst *Temp = Builder.CreateLoad(IT,AI);
            StoreInst *Restore = Builder.CreateStore(Temp,TLSReg);
            // errs() << "exitBlock whether dominate : " << DT.dominates(AI,Restore)<<" "+AI->getName()+" "+TLSReg->getName() <<"\n";
        }
    }
    for(Instruction *Inst:SkipFunctions) {
        Builder.SetInsertPoint(Inst);
        for(auto it = map.begin(); it != map.end(); it++) {
            auto AI = it->second;
            if(!map2.count(it->first())) continue; //因为如果涉及XMM，map1和map2的键值并非一一对应
            auto TLSReg = map2[it->first()];
            IntegerType *IT;
            if(XMMReg::isXMMReg(it->first())) {
                IT = Type::getInt128Ty(F.getContext());
            } else if(it->first().back() == 'F') {
                IT = Type::getInt8Ty(F.getContext());
            } else {
                IT = Type::getInt64Ty(F.getContext());
            }
            LoadInst *Temp = Builder.CreateLoad(IT,AI);
            StoreInst *Restore = Builder.CreateStore(Temp,TLSReg);
        }
        Builder.SetInsertPoint(Inst->getNextNode());
        for(auto it = map.begin(); it != map.end(); it++) {
            auto AI = it->second;
            if(!map2.count(it->first())) continue;
            auto TLSReg = map2[it->first()];
            IntegerType *IT;
            if(XMMReg::isXMMReg(it->first())) {
                IT = Type::getInt128Ty(F.getContext());
            } else if(it->first().back() == 'F') {
                IT = Type::getInt8Ty(F.getContext());
            } else {
                IT = Type::getInt64Ty(F.getContext());
            }
            LoadInst *Temp = Builder.CreateLoad(IT,TLSReg);
            StoreInst *Restore = Builder.CreateStore(Temp,AI);
        }
    }

    Changed = true;
    return Changed;
}

bool TLSImpl::Localize_TLS_Loop() {
    bool Changed = false;
    for(auto *L:make_early_inc_range(LI.getTopLevelLoopsVector())) {
        Changed |= simplifyLoop(L,&DT,&LI,&SE,nullptr,nullptr,false);
    }
    for(auto *L:LI.getTopLevelLoopsVector()) {
        errs()<<"is simplified loop : "<<L->isLoopSimplifyForm() <<"\n";
        Changed |= runOnLoop(L); 
    } 
    return Changed;
}

int TLSImpl::GetInfo(BasicBlock *BB,std::set<Value*> &TLSRegs,std::set<Instruction*> &SkipFunctions,int &UseTLSRegs) {
    int Insts = BB->size();
    for(auto &Inst:*BB) {
        // if(isa<LoadInst>(Inst) || isa<StoreInst>(Inst) || isa<PHINode>(Inst)) {
            for(Use &U:Inst.operands()) {
                Value *V = U.get();
                if(!V->hasName() || V->getName().size() <= 3) continue;
                StringRef name = V->getName();
                if(TLSPrefix.count(name.substr(0,name.find("_",0)))) {
                    TLSRegs.insert(V);
                    UseTLSRegs ++;
                }
            }
        // }
        if(CallInst *CI = dyn_cast<CallInst>(&Inst)) {
            Function *Callee = CI->getCalledFunction();
            if(Callee && Callee->hasName()) {
                if(SkipFunction(Callee->getName()) || !Callee->isIntrinsic()){
                    SkipFunctions.insert(CI);
                }
            } else {
                errs()<<"no name : "<<BB->getName()<<"\n";
                SkipFunctions.insert(CI);
            }
        }
    }
    return Insts;
}

bool TLSImpl::ShouldHandleBB(BasicBlock *BB,std::set<Value*> &TLSRegs,std::set<Instruction*> &SkipFunctions,int Insts,int UseTLSRegs) {
    std::set<StringRef> tar = {
    // "inst_42a2f0",
    // "inst_42c462",
    // "inst_42b1ab",
    // "inst_42b1c3",

    // "inst_42b178",
    // "inst_42b5cd",
    
    // "inst_42b1d4",
    // "inst_42b286",

    // "inst_42b275",
    // "inst_42b2f5",
    // "inst_42b369",
    // "inst_42b358",
    // "inst_42b3d4",
    // "inst_42b461",
    // "inst_42b450",
    // "inst_42b4d0",
    // "inst_42b544",
    // "inst_42b533",

    // "inst_42ba60",

    // "inst_429570"

    //XMM
    // "_ZN12_GLOBAL__N_110IDIVedxeaxI2RnIjLb1EEEEP6MemoryS4_R5StateT_2InImE.exit153",
    // "inst_47aeba",
    // "inst_40d100",
    // "inst_47a980",
//XMM3
    // "_ZN12_GLOBAL__N_110IDIVedxeaxI2MnIjEEEP6MemoryS4_R5StateT_2InImE.exit",


    // "inst_47b0ba",
    // "inst_432414",
    // "inst_41f390",
    "inst_4904d4",
    // "inst_4905b3",
    // "inst_49046b",
    // "inst_47be01",

    // "inst_47bc77",
    // "inst_47bf74",
    // "inst_47b909",
    // "inst_47bb87",
    // "inst_47bd0d",
    // "inst_47be84",
    // "inst_48a3de",
    // "inst_418c9f",
    // "inst_429570",

    };
    std::set<std::string> st;
    bool flag = false;
    for(auto V:TLSRegs) {
        if(XMMReg::isXMMReg(V->getName())) {
            st.insert(XMMReg(V->getName()).get_NameWithOff());
            flag = true;
            continue;
        }
        st.insert(std::string(V->getName().substr(0,V->getName().find("_"))));
    }
    errs()<<"BB Info: RegNum:"<<st.size()<<" SkipFunctionNum:"<<SkipFunctions.size()<<" UseReg:"<<UseTLSRegs<<" BBInsts:"<<Insts<<" hasXMM : "<<flag<<"\n";
    // if(BB->hasName() && tar.count(BB->getName())) return true; else return false;
    if(TLSRegs.empty()) {
        return false;
    }
    // return true;
    if(st.size() * 2 >= UseTLSRegs) {
        errs() << "BB : too few use\n";
        return false;
    }
    return true;

    if(SkipFunctions.empty()) {
        errs()<<"pass because there is no skip function\n";
        return true;
    }
    if(SkipFunctions.size() * st.size() * 2 > UseTLSRegs) {
        // errs() << "BB : too much skip functions "<<(st.size() >= 4) <<"\n";
        return false;
    }
    errs()<<"pass from here\n";
    return true;
}


bool TLSImpl::runOnBB(BasicBlock *BB) {
    bool Changed = false;
    std::set<Value*> TLSRegs;
    std::set<Instruction*> SkipFunctions;
    int UseTLSRegs = 0;
    int Insts = GetInfo(BB,TLSRegs,SkipFunctions,UseTLSRegs);
    if(!ShouldHandleBB(BB,TLSRegs,SkipFunctions,Insts,UseTLSRegs)) {
        return Changed;
    }
    // if(LI.getLoopFor(BB->getNextNode())) {
    //     return Changed;
    // }
    // if(skipBB.count(BB)){
    //     errs()<<"skipBB:"<<"\n";
    //     return Changed;
    // }   
    errs()<<"Handle BB : "<<BB->getName()<<"\n";

    // IRBuilder<> Builder(&*BB->getFirstInsertionPt());
    StringMap<Value*> map,map2;
    for(auto TLSReg : TLSRegs) {
        if(Loop *L = LI.getLoopFor(BB)) {
            Builder.SetInsertPoint(&L->getOutermostLoop()->getLoopPreheader()->back());
            // Builder.SetInsertPoint(F.getEntryBlock().getFirstInsertionPt());
        } else {
            Builder.SetInsertPoint(BB->getFirstNonPHIOrDbgOrAlloca());
            // Builder.SetInsertPoint(BB->getFirstInsertionPt());
        }
        StringRef Name = TLSReg->getName();
        if(XMMReg::isXMMReg(Name)) {
            std::string Local = std::string(std::string("Local_")+XMMReg(Name).get_NameWithOff());//XMM寄存器中分段的地址
            StringRef LocalBase = "Local_"+XMMReg(Name).get_Basename().str();//XMM寄存器的首地址
            // errs()<<"RegName : "<<Name<<" Local : "<<Local<<" LocalBase : "<<LocalBase<<"\n";
            Value *Local_ptr;
            if(map.count(Local)) {
                Local_ptr = map[Local];
            } else {
                Value *AI_Base;//局部XMM寄存器alloca的值
                if(map.count(LocalBase)) {
                    AI_Base = map[LocalBase];
                } else {
                    auto Ty128 = Type::getInt128Ty(F.getContext());
                    auto Ty64 = Type::getInt64Ty(F.getContext());
                    auto Ty8 = Type::getInt8Ty(F.getContext());
                    AI_Base = Builder.CreateAlloca(Builder.getInt32Ty(),Builder.getInt32(4),LocalBase);
                    // AI_Base = Builder.CreateAlloca(Ty128,nullptr,LocalBase);
                    // AI_Base = Builder.CreateAlloca(Ty8,ConstantInt::get(Ty64,16),LocalBase);
                    map.insert({LocalBase,AI_Base});
                    Value *regs_base = F.getParent()->getGlobalVariable("__mcsema_reg_state",true);
                    Value *reg_begin = Builder.CreateGEP(Ty8,regs_base,ConstantInt::get(Ty64,XMMReg(Name).get_offset()));
                    map2.insert({LocalBase,reg_begin});
                    LoadInst *LI = Builder.CreateLoad(Ty128,reg_begin,LocalBase);
                    Builder.CreateStore(LI,AI_Base);
                }
                auto Ty8 = Type::getInt8Ty(F.getContext());
                auto Ty64 = Type::getInt64Ty(F.getContext());
                // Value *temp = Builder.CreatePtrToInt(AI_Base,Builder.getIntPtrTy(F.getDataLayout()));
                // temp = Builder.CreateAdd(temp,ConstantInt::get(Ty64,XMMReg(Name).get_local_offset()));
                // Local_ptr = Builder.CreateIntToPtr(temp,Builder.getPtrTy(),Local);
                Builder.SetInsertPoint(cast<AllocaInst>(AI_Base)->getNextNode());
                Local_ptr = Builder.CreateGEP(Ty8,AI_Base,ConstantInt::get(Ty64,XMMReg(Name).get_local_offset()),Local);
                map.insert({Local,Local_ptr});
            }
            TLSReg->replaceUsesWithIf(Local_ptr,[&](Use &U) {
                User *user = U.getUser();
                if(Instruction *Inst = dyn_cast<Instruction>(user)) {
                    if(Inst->getParent() == BB && DT.dominates(cast<AllocaInst>(map[LocalBase]),Inst)) {
                        return true;
                    }
                    if(Inst->getParent() == BB && BB->hasName()) {
                        errs()<<"maybe a bug : "<<BB->getName() <<"\n";
                    }
                }
                return false;
            });
            continue;
        }
        StringRef Local = "Local_BB_"+std::string(Name.substr(0,Name.find("_")));
        Value *AI;
        Instruction *LoadI;
        bool exists = false;
        if(map.count(Local)) {
            AI = map[Local];
            exists = true;
        } else {
            IntegerType *IT;
            if(Local.back() == 'F') {
                IT = Type::getInt8Ty(F.getContext());
            } else {
                IT = Type::getInt64Ty(F.getContext());
            }
            AI = Builder.CreateAlloca(IT,nullptr,Local);
            // AI = Builder.CreateAlloca(Type::getInt8Ty(F.getContext()),ConstantInt::get(Type::getInt8Ty(F.getContext()),8),Local);
            map.insert({Local,AI});
            map2.insert({Local,TLSReg});
            if(LI.getLoopFor(BB)) {
                Builder.SetInsertPoint(BB->getFirstNonPHIOrDbgOrAlloca());
            }
            LoadI = Builder.CreateLoad(IT,TLSReg);
            StoreInst *SI = Builder.CreateStore(LoadI,AI);
        }
        // errs()<<TLSReg->getNumUses()<<" "<<"\n";
        TLSReg->replaceUsesWithIf(AI,[&](Use &U){
            User *user = U.getUser();
            if(Instruction *Inst = dyn_cast<Instruction>(user)) {
                if(Inst != LoadI && Inst->getParent() == BB && DT.dominates(cast<AllocaInst>(AI),Inst)) {
                    // errs() << "inLoop whether dominate : " << AI->getName() <<" "<<Inst->getParent()->getName() <<"\n";
                    return true;
                }
            }
            return false;
        });
        // if(exists) continue;
        // Builder.SetInsertPoint(&BB->back());
        // IntegerType *IT;
        // if(Local.back() == 'F') {
        //     IT = Type::getInt8Ty(F.getContext());
        // } else {
        //     IT = Type::getInt64Ty(F.getContext());
        // }
        // LoadInst *Temp = Builder.CreateLoad(IT,AI);
        // StoreInst *Restore = Builder.CreateStore(Temp,TLSReg);
    }

    for(auto &[Local,AI]:map) {
        Builder.SetInsertPoint(&BB->back());
        if(!map2.count(Local)) continue;
        auto TLSReg = map2[Local];
        IntegerType *IT;
        if(XMMReg::isXMMReg(Local)) {
            IT = Type::getInt128Ty(F.getContext());
        } else if(Local.back() == 'F') {
            IT = Type::getInt8Ty(F.getContext());
        } else {
            IT = Type::getInt64Ty(F.getContext());
        }
        LoadInst *Temp = Builder.CreateLoad(IT,AI);
        StoreInst *Restore = Builder.CreateStore(Temp,TLSReg);
    }

    for(Instruction *Inst:SkipFunctions) {
        Builder.SetInsertPoint(Inst);
        for(auto it = map.begin(); it != map.end(); it++) {
            auto AI = it->second;
            if(!map2.count(it->first())) continue;
            auto TLSReg = map2[it->first()];
            IntegerType *IT;
            if(XMMReg::isXMMReg(it->first())) {
                IT = Type::getInt128Ty(F.getContext());
            } else if(it->first().back() == 'F') {
                IT = Type::getInt8Ty(F.getContext());
            } else {
                IT = Type::getInt64Ty(F.getContext());
            }
            LoadInst *Temp = Builder.CreateLoad(IT,AI);
            StoreInst *Restore = Builder.CreateStore(Temp,TLSReg);
        }
        Builder.SetInsertPoint(Inst->getNextNode());
        for(auto it = map.begin(); it != map.end(); it++) {
            auto AI = it->second;
            if(!map2.count(it->first())) continue;
            auto TLSReg = map2[it->first()];
            IntegerType *IT;
            if(XMMReg::isXMMReg(it->first())) {
                IT = Type::getInt128Ty(F.getContext());
            } else if(it->first().back() == 'F') {
                IT = Type::getInt8Ty(F.getContext());
            } else {
                IT = Type::getInt64Ty(F.getContext());
            }
            LoadInst *Temp = Builder.CreateLoad(IT,TLSReg);
            StoreInst *Restore = Builder.CreateStore(Temp,AI);
        }
    }
    Changed = true;
    return Changed;
}


bool TLSImpl::Localize_TLS_BB() {
    bool Changed = false;
    for(auto *L:make_early_inc_range(LI.getTopLevelLoopsVector())) {
        Changed |= simplifyLoop(L,&DT,&LI,&SE,nullptr,nullptr,false);
    }
    for(auto &BB:make_range(F.begin(),F.end())) {
        if(LI.getLoopFor(&BB)) {
            continue;
        }
        Changed |= runOnBB(&BB);
    }
    return Changed;
}
}

namespace {
class TLSIRPass : public PassInfoMixin<TLSIRPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        TLSImpl impl(F,AM);
        bool Changed = false;
        if(!impl.ShouldHandle()) {
            errs()<<"Skip Function : "<<F.getName()<<"\n";
            return PreservedAnalyses::all();
        }
        
        errs()<<"--------- Handle Function Begin : "<<F.getName()<<" ---------\n";
        Changed |= impl.Localize_TLS_Loop();
        Changed |= impl.Localize_TLS_BB();
        // F.dump();
        // F.viewCFG();
        errs()<<"--------- Handle Function End : "<<F.getName()<<" ---------\n";
        if(Changed) {
            return PreservedAnalyses::none();
        } else {
            return PreservedAnalyses::all();
        }
    }
};
}

void RegisterCB(PassBuilder &PB) {
    PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM, 
                ArrayRef<PassBuilder::PipelineElement>) {
            if (Name == "tls") {
                FPM.addPass(TLSIRPass());
                return true;
            }
            return false;
        }
    );
    // PB.registerVectorizerStartEPCallback(
    //     [](FunctionPassManager &FPM, OptimizationLevel Level) {
    //         FPM.addPass(TLSIRPass());
    //     }
    // );
}

PassPluginLibraryInfo getTLSPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "TLS", "v1", RegisterCB};
}

#ifndef LLVM_TLS_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getTLSPluginInfo();
}
#endif