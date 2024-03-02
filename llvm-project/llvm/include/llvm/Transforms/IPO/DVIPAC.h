// DVIPACPass.h - DVI-PAC project - COSMOSS
//
//
// Author: Mohannad Ismail
#ifndef LLVM_IPO_DVIPAC
#define LLVM_IPO_DVIPAC

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
	
	struct InlineParams;
	class StringRef;
	class ModuleSummaryIndex;
	class ModulePass;
	class Pass;
	class Function;
	class BasicBlock;
	class GlobalValue;
	class raw_ostream;

	ModulePass *createDVIPACPass();

	class DVIPACPass : public PassInfoMixin<DVIPACPass>{
		PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);	
	};
} //namespace llvm

#endif //LLVM_IPO_DVIPAC
