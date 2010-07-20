//===- AliasDebugger.cpp - Simple Alias Analysis Use Checker --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This simple pass checks alias analysis users to ensure that if they
// create a new value, they do not query AA without informing it of the value.
// It acts as a shim over any other AA pass you want.
//
// Yes keeping track of every value in the program is expensive, but this is 
// a debugging pass.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/Passes.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include <set>
using namespace llvm;

namespace {
  
  class AliasDebugger : public ModulePass, public AliasAnalysis {

    //What we do is simple.  Keep track of every value the AA could
    //know about, and verify that queries are one of those.
    //A query to a value that didn't exist when the AA was created
    //means someone forgot to update the AA when creating new values

    std::set<const Value*> Vals;
    
  public:
    static char ID; // Class identification, replacement for typeinfo
    AliasDebugger() : ModulePass(&ID) {}

    bool runOnModule(Module &M) {
      InitializeAliasAnalysis(this);                 // set up super class

      for(Module::global_iterator I = M.global_begin(),
            E = M.global_end(); I != E; ++I) {
        Vals.insert(&*I);
        for (User::const_op_iterator OI = I->op_begin(),
             OE = I->op_end(); OI != OE; ++OI)
          Vals.insert(*OI);
      }

      for(Module::iterator I = M.begin(),
            E = M.end(); I != E; ++I){
        Vals.insert(&*I);
        if(!I->isDeclaration()) {
          for (Function::arg_iterator AI = I->arg_begin(), AE = I->arg_end();
               AI != AE; ++AI) 
            Vals.insert(&*AI);     
          for (Function::const_iterator FI = I->begin(), FE = I->end();
               FI != FE; ++FI) 
            for (BasicBlock::const_iterator BI = FI->begin(), BE = FI->end();
                 BI != BE; ++BI) {
              Vals.insert(&*BI);
              for (User::const_op_iterator OI = BI->op_begin(),
                   OE = BI->op_end(); OI != OE; ++OI)
                Vals.insert(*OI);
            }
        }
        
      }
      return false;
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AliasAnalysis::getAnalysisUsage(AU);
      AU.setPreservesAll();                         // Does not transform code
    }

    /// getAdjustedAnalysisPointer - This method is used when a pass implements
    /// an analysis interface through multiple inheritance.  If needed, it
    /// should override this to adjust the this pointer as needed for the
    /// specified pass info.
    virtual void *getAdjustedAnalysisPointer(const StaticPassInfo *PI) {
      if (PI->isPassID(&AliasAnalysis::ID))
        return (AliasAnalysis*)this;
      return this;
    }
    
    //------------------------------------------------
    // Implement the AliasAnalysis API
    //
    AliasResult alias(const Value *V1, unsigned V1Size,
                      const Value *V2, unsigned V2Size) {
      assert(Vals.find(V1) != Vals.end() && "Never seen value in AA before");
      assert(Vals.find(V2) != Vals.end() && "Never seen value in AA before");    
      return AliasAnalysis::alias(V1, V1Size, V2, V2Size);
    }

    ModRefResult getModRefInfo(CallSite CS, Value *P, unsigned Size) {
      assert(Vals.find(P) != Vals.end() && "Never seen value in AA before");
      return AliasAnalysis::getModRefInfo(CS, P, Size);
    }

    ModRefResult getModRefInfo(CallSite CS1, CallSite CS2) {
      return AliasAnalysis::getModRefInfo(CS1,CS2);
    }
    
    bool pointsToConstantMemory(const Value *P) {
      assert(Vals.find(P) != Vals.end() && "Never seen value in AA before");
      return AliasAnalysis::pointsToConstantMemory(P);
    }

    virtual void deleteValue(Value *V) {
      assert(Vals.find(V) != Vals.end() && "Never seen value in AA before");
      AliasAnalysis::deleteValue(V);
    }
    virtual void copyValue(Value *From, Value *To) {
      Vals.insert(To);
      AliasAnalysis::copyValue(From, To);
    }

  };
}

char AliasDebugger::ID = 0;
static RegisterPass<AliasDebugger>
X("debug-aa", "AA use debugger", false, true);
static RegisterAnalysisGroup<AliasAnalysis> Y(X);

Pass *llvm::createAliasDebugger() { return new AliasDebugger(); }

