// DVIPACPass.cpp - STI project - COSMOSS
//
//
// Author: Mohannad Ismail

#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/CtorUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/TargetFolder.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Type.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/IPO/DVIPAC.h"

#include <cstdint>
#include <utility>
#include <vector>
#include <tuple>
#include <unordered_map>

//#define STC_PAC
#define STC_EOR
//#define STWC_PAC
#define STWC_EOR
//#define STL_PAC
#define STL_EOR

using namespace llvm;
using std::unordered_map;
using std::make_pair;
using std::make_tuple;
using std::vector;
using std::get;
using std::tuple;
using std::pair;
using std::unordered_multimap;
using IRBuilderTy = llvm::IRBuilder<>;


// String references for the functions.
StringRef INITMEMORY("init_memory");
StringRef ADDPAC("addPACpp");
StringRef AUTHPAC("authenticatePACpp");
StringRef SETMETA("setMetadata");
StringRef ADDCETBI("addCEtbi");
StringRef METAEXISTS("metadataExists");


#define DEBUG_TYPE "DVIPACPass"

// Initialize DVI-PAC functions so that they don't get eliminated.
bool initDVIPAC = false;

// Map to store metadata set. Key = Pointer type; Value = Scope of use (which function/s the type is used in) , permission, value
struct TypeMeta{
	std::vector<StringRef> scope;
	int permission;
	int modifier;
	std::vector<StringRef> variables;

	TypeMeta(): scope(std::vector<StringRef>()), permission(0), modifier(0), variables(std::vector<StringRef>())
	{ } 
};
unordered_map<Type*, std::vector<TypeMeta>> MetaSet = unordered_map<Type*, std::vector<TypeMeta>>();

// Global counter for modifier values
int mod = 1;

// Cache for tracking non-escaping local objects.
SmallDenseMap<const Value*, bool, 8> IsCapturedCache;

// Named MDNode definition for collecting metadata and transferring it to the MIR pass
NamedMDNode* NN;

// Handling globals
bool globalsDone = false;

// Pointer-to-pointer handling data structures
std::unordered_map<Type*, unsigned int> P2PTypes;			
unsigned int tbi_tag = 1;
unsigned int type_id = 1;


SmallDenseMap<const Value*, bool, 8> ICC;

namespace llvm {
	class DVIPACInstr : public ModulePass {
	public:
		static char ID;
		DVIPACInstr() : ModulePass(ID) {
			initializeDVIPACInstrPass(*PassRegistry::getPassRegistry());
		}
		~DVIPACInstr() { }

		bool doInitialization(Module &M) override {
			return false;
		}

		// Print stats
		static void printStat(raw_ostream &OS, Statistic &S){
			OS << format("%8u %s - %s\n", S.getValue(), S.getName(), S.getDesc());
		}

		// Insert Set Meta Library call. This is because setMetadata takes three arguements.
        void insertSetMetaLibCall(unsigned int type_id, Instruction* InsertPointInst, StringRef FunctionName, unsigned int tbi_tag, unsigned int mod){
            LLVMContext &C = InsertPointInst->getFunction()->getContext();
			Value* TypeId = ConstantInt::get(Type::getInt64Ty(C), type_id);
            Value* TbiTag = ConstantInt::get(Type::getInt64Ty(C), tbi_tag);
            Value* Mod = ConstantInt::get(Type::getInt64Ty(C), mod);
            IRBuilder<> builder(C);

            builder.SetInsertPoint(InsertPointInst);

            ArrayRef<Type*> FuncParams = {
                TypeId->getType(),
                TbiTag->getType(),
                Mod->getType()
            };

            Type* ResultFTy = Type::getVoidTy(C);
            FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);

            FunctionCallee Func = InsertPointInst->getModule()->getOrInsertFunction(FunctionName, FTy);
            ArrayRef<Value*> FuncArgs = {
                TypeId,
                TbiTag,
                Mod
            };

            builder.CreateCall(Func, FuncArgs);
        }

		// Insert Library call before instruction.
        CallInst* insertLibCallBefore(Value* ArgVal, Instruction* InsertPointInst, StringRef FunctionName, unsigned int tbi_tag){
            LLVMContext &C = InsertPointInst->getFunction()->getContext();
            IRBuilder<> builder(C);

            Type* Ty = Type::getInt8Ty(C)->getPointerTo();
			Value* TbiTag = ConstantInt::get(Type::getInt64Ty(C), tbi_tag);
            // Insertion point after passed instruction
            builder.SetInsertPoint(InsertPointInst);

            ArrayRef<Type*> FuncParams = {
                Ty->getPointerTo(),
				TbiTag->getType()
            };

            Type* ResultFTy = Type::getVoidTy(C);
            FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);

            FunctionCallee Func = InsertPointInst->getModule()->getOrInsertFunction(FunctionName, FTy);
            ArrayRef<Value*> FuncArgs = {
                builder.CreatePointerCast(ArgVal, Ty->getPointerTo()),
				TbiTag
            };

            return builder.CreateCall(Func, FuncArgs);
        }


        // Insert Library call after instruction.
        CallInst* insertLibCallAfter(Value* ArgVal, Instruction* InsertPointInst, StringRef FunctionName, unsigned int tbi_tag){
            LLVMContext &C = InsertPointInst->getFunction()->getContext();
            IRBuilder<> builder(C);

            Type* Ty = Type::getInt8Ty(C)->getPointerTo();
			Value* TbiTag = ConstantInt::get(Type::getInt64Ty(C), tbi_tag);
            // Insertion point after passed instruction
            builder.SetInsertPoint(InsertPointInst->getNextNode());

            ArrayRef<Type*> FuncParams = {
                Ty->getPointerTo(),
				TbiTag->getType()
            };

            Type* ResultFTy = Type::getVoidTy(C);
            FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);

            FunctionCallee Func = InsertPointInst->getModule()->getOrInsertFunction(FunctionName, FTy);
            ArrayRef<Value*> FuncArgs = {
                builder.CreatePointerCast(ArgVal, Ty->getPointerTo()),
				TbiTag
            };

            return builder.CreateCall(Func, FuncArgs);
        }

		// Check if type is FP or drills down to FP
        bool isFPType(Type* Ty){
            if (Ty->isPointerTy() && cast<PointerType>(Ty)->getElementType()->isFunctionTy()){
                return true;
            }

            // Generic pointer - Recursively find out if it's a FP
            else if (PointerType* PTy = dyn_cast<PointerType>(Ty)){
                Type* ETy = PTy->getElementType();
                return isFPType(ETy);
            }

            return false;
        }

		Type* getActualType(Type* Ty){
			if (PointerType* PTyD = dyn_cast<PointerType>(Ty->getPointerElementType())){
				return getActualType(PTyD);
			}
			else {
				return Ty->getPointerElementType();
			}
			return Ty->getPointerElementType();
		}

		StringRef getVariableNameNext(Instruction* I){
			if(I->getNextNode() == nullptr){
				return "";
			}
			StringRef variable;
			StringRef variable_def = "default_variable_name";
			I = I->getNextNode();
			if (CallInst* CI = dyn_cast<CallInst>(I)){
				Function* CF = CI->getCalledFunction();
				if(CF && isLLVMMetaFunction(CF->getName())){
					if(Metadata *MetaCV = dyn_cast<MetadataAsValue>(I->getOperand(1))->getMetadata()){
						if(DILocalVariable *DIMetaCV = dyn_cast<DILocalVariable>(MetaCV)){
							variable = DIMetaCV->getName();
						}
					}
				}
				else{
					variable = getVariableNameNext(I);
				}
			}
			else{
				variable = getVariableNameNext(I);
			}
			return variable_def;
		}

		Type* getTypeNext(Instruction* I){
			if(I->getNextNode() == nullptr){
				return nullptr;
			}
			Type* Ty = nullptr;
			I = I->getNextNode();
			if (CallInst* CI = dyn_cast<CallInst>(I)){
				Function* CF = CI->getCalledFunction();
				if(CF && isLLVMMetaFunction(CF->getName())){
					if(Metadata *MetaCV = dyn_cast<MetadataAsValue>(I->getOperand(0))->getMetadata()){
						if(DIArgList *MetaDIArg = dyn_cast<DIArgList>(MetaCV)){
							return nullptr;
						}
						if(ValueAsMetadata *DIMetaC = dyn_cast<ValueAsMetadata>(MetaCV)){
							Value* DIMetaCV = DIMetaC->getValue();
							Ty = DIMetaCV->getType();
						}
					}
				}
				else{
					Ty = getTypeNext(I);
				}
			}
			else{
				Ty = getTypeNext(I);
			}
			return Ty;
		}

		StringRef getVariableNamePrev(Instruction* I){
			if(I->getPrevNode() == nullptr){
				return "";
			}
			StringRef variable;
			StringRef variable_def = "default_variable_name";
			I = I->getPrevNode();
			if (CallInst* CI = dyn_cast<CallInst>(I)){
				Function* CF = CI->getCalledFunction();
				if(CF && isLLVMMetaFunction(CF->getName())){
					if(Metadata *MetaCV = dyn_cast<MetadataAsValue>(I->getOperand(1))->getMetadata()){
						if(DILocalVariable *DIMetaCV = dyn_cast<DILocalVariable>(MetaCV)){
							variable = DIMetaCV->getName();
						}
					}
				}
				else{
					variable = getVariableNamePrev(I);
				}
			}
			else{
				variable = getVariableNamePrev(I);
			}
			return variable_def;
		}

		Type* getTypePrev(Instruction* I){
			if(I->getPrevNode() == nullptr){
				return nullptr;
			}
			Type* Ty = nullptr;
			I = I->getPrevNode();
			if (BitCastInst* BI = dyn_cast<BitCastInst>(I)){
				Type* srcTy = BI->getSrcTy();
				if(srcTy->isPointerTy()){
					if (srcTy->getPointerElementType()->isPointerTy()){
						Ty = srcTy->getPointerElementType();
					}
					else{
						Ty = srcTy;
					}
				}
				else{
					return Ty;
				}
			}
			else if (CallInst* CI = dyn_cast<CallInst>(I)){
				Function* CF = CI->getCalledFunction();
				if(CF && isLLVMMetaFunction(CF->getName())){
					if(Metadata *MetaCV = dyn_cast<MetadataAsValue>(I->getOperand(0))->getMetadata()){
   						if(DIArgList *MetaDIArg = dyn_cast<DIArgList>(MetaCV)){
							return nullptr;
						}
						if(Value *DIMetaCV = dyn_cast<ValueAsMetadata>(MetaCV)->getValue()){
							Ty = DIMetaCV->getType();
						}
					}
				}
				else{
					Ty = getTypePrev(I);
				}
			}
			else{
				Ty = getTypePrev(I);
			}
			return Ty;
		}


		
		int fetchModifier(Type* Ty, StringRef variable, StringRef scope){
			int mod = 24;
			if (Ty == nullptr){
				return 24;
			}
			if (MetaSet.find(Ty) == MetaSet.end()){
				return 24;
			}
			else{
				auto modMeta = MetaSet.find(Ty);
							
				if(std::find(modMeta->second[0].scope.begin(), modMeta->second[0].scope.end(), scope) != modMeta->second[0].scope.end()){
					if(std::find(modMeta->second[0].variables.begin(), modMeta->second[0].variables.end(), variable) != modMeta->second[0].variables.end()){
						mod = modMeta->second[0].modifier;
					}
				}

				if(std::find(modMeta->second[1].scope.begin(), modMeta->second[1].scope.end(), scope) != modMeta->second[1].scope.end()){
					if(std::find(modMeta->second[1].variables.begin(), modMeta->second[1].variables.end(), variable) != modMeta->second[1].variables.end()){
						mod = modMeta->second[1].modifier;
					}
				}

				if(std::find(modMeta->second[2].scope.begin(), modMeta->second[2].scope.end(), scope) != modMeta->second[2].scope.end()){
					if(std::find(modMeta->second[2].variables.begin(), modMeta->second[2].variables.end(), variable) != modMeta->second[2].variables.end()){
						mod = modMeta->second[2].modifier;
					}
				}
				
			}
			return mod;
		}

		void handleBitcastsSTC(Function &F){
			if (isLibPAC(F.getName())){
				return;
			}
            for (Function::iterator FIt = F.begin(); FIt != F.end(); ++FIt){
                BasicBlock *BB = &*FIt;
                for (BasicBlock::iterator BBIt = BB->begin(); BBIt != BB->end(); ++BBIt){
                    Instruction *I = &*BBIt;
                    if (isa<PHINode>(I) || isa<LandingPadInst>(I)){
                        continue;
                    }
					if(BitCastInst* BI = dyn_cast<BitCastInst>(I)){
						Type* srcTy = BI->getSrcTy();
						Type* destTy = BI->getDestTy();

						if (MetaSet.find(srcTy) != MetaSet.end()){
							if(MetaSet.find(destTy) != MetaSet.end()){
								auto srcMeta = MetaSet.find(srcTy);
								auto dstMeta = MetaSet.find(destTy);

								
								if(dstMeta->second[0].modifier != 0){ 
									dstMeta->second[0].modifier = 24;
								}
								if(srcMeta->second[0].modifier != 0){ 
									srcMeta->second[0].modifier = 24;
								}
								if(dstMeta->second[1].modifier != 0){ 
									dstMeta->second[1].modifier = 24;
								}
								if(srcMeta->second[1].modifier != 0){ 
									srcMeta->second[1].modifier = 24;
								}
								if(dstMeta->second[2].modifier != 0){ 
									dstMeta->second[2].modifier = 24;
								}
								if(srcMeta->second[2].modifier != 0){ 
									srcMeta->second[2].modifier = 24;
								}
							}
						}
					}

					if (CallInst *CI = dyn_cast<CallInst>(I)){
						Function* CF = CI->getCalledFunction();
						if(CF && isLLVMMetaFunction(CF->getName())){
							if(Metadata *MetaCVTy = dyn_cast<MetadataAsValue>(I->getOperand(0))->getMetadata()){
								if(DIArgList *MetaDIArg = dyn_cast<DIArgList>(MetaCVTy)){
                 	               continue;
                        	    }
								if(!isa<ValueAsMetadata>(MetaCVTy)){
                	                continue;
            	                }
								if(Value* VTy = dyn_cast<ValueAsMetadata>(MetaCVTy)->getValue()){
									Type* MetaType = VTy->getType();
									if(!MetaType->isPointerTy()){
										continue;
									}
									Type* MetaTypeFetch = MetaType->getPointerElementType();
									if(MetaTypeFetch->isPointerTy()){
										if(MetaSet.find(MetaType) != MetaSet.end()){
											if(MetaSet.find(MetaTypeFetch) != MetaSet.end()){
												auto MetaTypeMD = MetaSet.find(MetaType);
												auto MetaTypeFetchMD = MetaSet.find(MetaTypeFetch);
												
												if(MetaTypeFetchMD->second[0].modifier != 0 && MetaTypeMD->second[0].modifier != 0){
													MetaTypeFetchMD->second[0].modifier = MetaTypeMD->second[0].modifier;
												}
												if(MetaTypeFetchMD->second[1].modifier != 0 && MetaTypeMD->second[1].modifier != 0){
													MetaTypeFetchMD->second[1].modifier = MetaTypeMD->second[1].modifier;
												}
												if(MetaTypeFetchMD->second[2].modifier != 0 && MetaTypeMD->second[2].modifier != 0){
													MetaTypeFetchMD->second[2].modifier = MetaTypeMD->second[2].modifier;
												}	
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		void handleBitcastsSTWC(Function &F){
			if (isLibPAC(F.getName())){
				return;
			}
            for (Function::iterator FIt = F.begin(); FIt != F.end(); ++FIt){
                BasicBlock *BB = &*FIt;
                for (BasicBlock::iterator BBIt = BB->begin(); BBIt != BB->end(); ++BBIt){
                    Instruction *I = &*BBIt;
					LLVMContext &C = I->getFunction()->getContext();
                    DataLayout DL = DataLayout(I->getModule());
                    if (isa<PHINode>(I) || isa<LandingPadInst>(I)){
                        continue;
                    }

					if(BitCastInst* BI = dyn_cast<BitCastInst>(I)){
						if (BI->getName().contains("stwcbi")){
							continue;
						}
						Type* srcTy = BI->getSrcTy();
						Type* destTy = BI->getDestTy();

						if (MetaSet.find(srcTy) != MetaSet.end()){
							if(MetaSet.find(destTy) != MetaSet.end()){

                        	    DataLayout DL = DataLayout(I->getModule());

								IRBuilder<> builder(C);
                            	Value* key = llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 2);

								auto srcModMeta = MetaSet.find(srcTy);
                            	int srcMod = srcModMeta->second[0].modifier;
								Value* srcPointer = BI->getOperand(0);
                            	Value* srcModifier = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), srcMod);
								builder.SetInsertPoint(I->getNextNode());
	                            srcPointer = builder.CreatePtrToInt(srcPointer, builder.getIntPtrTy(DL));
								Value* srcRHS = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 140737488355328);
    	                        Value* srcResult = builder.CreateICmpUGE(srcPointer, srcRHS);
	                            Instruction* src_inst_result = cast<Instruction>(srcResult);
#ifdef STWC_PAC
								Instruction* srcSPL = SplitBlockAndInsertIfThen(srcResult, src_inst_result->getNextNode(), false);
								builder.SetInsertPoint(srcSPL);
								ArrayRef<Type*> srcFuncParams = {
    	                            Type::getInt64Ty(C),
	                                Type::getInt32Ty(C),
                                	Type::getInt64Ty(C)
                            	};

								Type* srcResultFTy = Type::getInt64Ty(C);
	                            FunctionType* srcFTy = FunctionType::get(srcResultFTy, srcFuncParams, false);
    	                        FunctionCallee srcFunc = I->getModule()->getOrInsertFunction("llvm.ptrauth.auth.i64", srcFTy);
        	                    ArrayRef<Value*> srcFuncArgs = {
            	                    srcPointer,
                	                key,
                    	            srcModifier
                        	    };

								srcPointer = builder.CreateCall(srcFunc, srcFuncArgs);
	                            srcPointer = builder.CreateIntToPtr(srcPointer, destTy);
								Value* pointerBI_addr = builder.CreatePointerCast(BI->getOperand(0), destTy->getPointerTo(), "stwcbi");
								builder.CreateStore(srcPointer, pointerBI_addr);
#endif
#ifdef STWC_EOR	
            	                StringRef asmString = "eor x18, x18, x18\n";
        	                    StringRef constraints = "";
    	                        FunctionType* FTy = FunctionType::get(Type::getVoidTy(C), false);
	
	                            InlineAsm *IA = InlineAsm::get(FTy, asmString, constraints, false);
                            	ArrayRef<Value *> Args = None;
                        	    builder.CreateCall(IA, Args);
                    	        builder.CreateCall(IA, Args);
                	            builder.CreateCall(IA, Args);
            	                builder.CreateCall(IA, Args);
        	                    builder.CreateCall(IA, Args);
    	                        builder.CreateCall(IA, Args);
	                            builder.CreateCall(IA, Args);
								
								Value* pointerBIL_addr = builder.CreatePointerCast(BI->getOperand(0), srcTy->getPointerTo(), "stwcbi");
								builder.CreateLoad(srcTy, pointerBIL_addr);
#endif
							}
						}
					}				
				}
			}
		}


		void insertModifierMeta(Function &F){
			if (isLibPAC(F.getName())){
				return;
			}
			bool isGlobal = false;
			int mod = 24;
			Type* Ty = nullptr;
			Type* RTy = nullptr;
			StringRef variable_name;
			StringRef scope;
			for (Function::iterator FIt = F.begin(); FIt != F.end(); ++FIt){
				BasicBlock *BB = &*FIt;
                for (BasicBlock::iterator BBIt = BB->begin(); BBIt != BB->end(); ++BBIt){
                    Instruction *I = &*BBIt;
					if (isa<PHINode>(I) || isa<LandingPadInst>(I)){
                       	continue;
                   	}
					LLVMContext &C = I->getContext();
					if (StoreInst* SI = dyn_cast<StoreInst>(I)){
						Ty = SI->getValueOperand()->getType();
						if(isFPType(Ty)){
							mod = 24;
							MDNode* N = MDNode::get(C, ConstantAsMetadata::get(ConstantInt::get(C, llvm::APInt(64, mod, false))));
							I->setMetadata("mod", N);
							continue;
						}
						if (Ty->isPointerTy()){
							if(ConstantExpr* CI = dyn_cast<ConstantExpr>(SI->getPointerOperand())){
								if(GlobalVariable* GV = dyn_cast<GlobalVariable>(CI->getOperand(0))){
									variable_name = GV->getName();
									SmallVector<std::pair<unsigned, MDNode *>, 4> GMDs;
                                	GV->getAllMetadata(GMDs);
                                	MDNode *GMeta = nullptr;
									for (auto &GMD : GMDs){
										GMeta = GMD.second;
										break;
									}
									RTy = Ty;
									isGlobal = true;
								}
								else{
									variable_name = getVariableNamePrev(SI);
									RTy = getTypePrev(SI);
								}
							}
							else if(GlobalVariable* GVI = dyn_cast<GlobalVariable>(SI->getPointerOperand())){
								variable_name = GVI->getName();
								SmallVector<std::pair<unsigned, MDNode *>, 4> GMDs;
                               	GVI->getAllMetadata(GMDs);
                               	MDNode *GMeta = nullptr;
								for (auto &GMD : GMDs){
									GMeta = GMD.second;
									break;
								}
								RTy = Ty;
								isGlobal = true;
							}
							else if (BitCastInst* BI = dyn_cast<BitCastInst>(SI->getPointerOperand())){
								Type* srcTy = BI->getSrcTy();
								if(srcTy->isPointerTy()){
									if(srcTy->getPointerElementType()->isPointerTy()){
										Ty = srcTy->getPointerElementType();
										RTy = Ty;
									}
									else{
										Ty = srcTy;
										RTy = Ty;
									}
								}
								else{
									RTy = Ty;
								}
							}
							else{
								variable_name = getVariableNamePrev(SI);
								RTy = getTypePrev(SI);
								if(RTy == nullptr){
									RTy = Ty;
								}
							}
						}
						else{
							continue;
						}
					}
					else if (LoadInst* LI = dyn_cast<LoadInst>(I)){
						Ty = LI->getType();
						if(isFPType(Ty)){
							mod = 24;
							MDNode* N = MDNode::get(C, ConstantAsMetadata::get(ConstantInt::get(C, llvm::APInt(64, mod, false))));
							I->setMetadata("mod", N);
							continue;
						}
						if (Ty->isPointerTy()){
							if(GlobalVariable* GV = dyn_cast<GlobalVariable>(LI->getPointerOperand())){
								variable_name = GV->getName();
								SmallVector<std::pair<unsigned, MDNode *>, 4> GMDs;
                               	GV->getAllMetadata(GMDs);
                               	MDNode *GMeta = nullptr;
								for (auto &GMD : GMDs){
									GMeta = GMD.second;
									break;
								}
								RTy = Ty;
								isGlobal = true;
							}
							else{
								variable_name = getVariableNameNext(LI);
								if(variable_name == ""){
									variable_name = getVariableNamePrev(LI);
								}
								RTy = getTypeNext(LI);
								if (RTy == nullptr){
									RTy = getTypePrev(LI);
									if (RTy == nullptr){
										RTy = Ty;
									}
								}
							}
						}
						else{
							continue;
						}
					}
					else{
						continue;
					}

					// Get scope
					if(DILocation* DIL = I->getDebugLoc()){
						DILocalScope* DIScope = DIL->getScope();
						scope = DIScope->getName();
					}
					if(isGlobal == true){
						scope = "global";
					}
					mod = fetchModifier(RTy, variable_name, scope);
					MDNode* N = MDNode::get(C, ConstantAsMetadata::get(ConstantInt::get(C, llvm::APInt(64, mod, false))));
					I->setMetadata("mod", N);
				}
			}
		}
		
		void llvmMetadataFetch(Module &M){
   			std::vector<TypeMeta> initTypeMetaV;
			initTypeMetaV.push_back(TypeMeta());
			initTypeMetaV[0].scope = std::vector<StringRef>();
			initTypeMetaV[0].permission = 0;
			initTypeMetaV[0].modifier = 0;
			initTypeMetaV[0].variables = std::vector<StringRef>();
			initTypeMetaV.push_back(TypeMeta());
			initTypeMetaV[1].scope = std::vector<StringRef>();
			initTypeMetaV[1].permission = 0;
			initTypeMetaV[1].modifier = 0;
			initTypeMetaV[1].variables = std::vector<StringRef>();
			initTypeMetaV.push_back(TypeMeta());
			initTypeMetaV[2].scope = std::vector<StringRef>();
			initTypeMetaV[2].permission = 0;
			initTypeMetaV[2].modifier = 0;
			initTypeMetaV[2].variables = std::vector<StringRef>();

            for (auto &F : M){
				if (isLibPAC(F.getName())){
					continue;
				}
            for (Function::iterator FIt = F.begin(); FIt != F.end(); ++FIt){
                BasicBlock *BB = &*FIt;
                for (BasicBlock::iterator BBIt = BB->begin(); BBIt != BB->end(); ++BBIt){
                    Instruction *I = &*BBIt;
					if (isa<PHINode>(I) || isa<LandingPadInst>(I)){
                       	continue;
                   	}
					LLVMContext &C = I->getContext();
					Module* M = I->getModule();
					SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
					I->getAllMetadata(MDs);
					MDNode *Meta = nullptr;
					for (auto &MD : MDs){
						Meta = MD.second;
						break;
					}
					Type* MetaTy = nullptr;

					StringRef MetaScope;
					int permission = 0;
					if (Meta != nullptr){
						//SCOPE
						if(DISubprogram *MetaScopeDI = dyn_cast<DISubprogram>(Meta->getOperand(0))){
							MetaScope = MetaScopeDI->getName();
						}
						else if (DILexicalBlock *MetaLex = dyn_cast<DILexicalBlock>(Meta->getOperand(0))){
							DISubprogram *MetaLexDI = MetaLex->getScope()->getSubprogram();
							MetaScope = MetaLexDI->getName();
						}
					}

					//Globals
					if (globalsDone == false){
						for (auto GIt = M->global_begin(); GIt != M->global_end(); ++GIt){
							GlobalVariable* GV = dyn_cast<GlobalVariable>(&*GIt);
							if(GV->hasPrivateLinkage()){
								continue;
							}
							if(GV && GV->hasInitializer()){
								Type* GMetaTy = GV->getInitializer()->getType();
								if(!GMetaTy->isPointerTy()){
                                	continue;
                            	}
								StringRef GMetaScope = "global";
								SmallVector<std::pair<unsigned, MDNode *>, 4> GMDs;
								GV->getAllMetadata(GMDs);
								MDNode *GMeta = nullptr;
								for (auto &GMD : GMDs){
									GMeta = GMD.second;
									break;
								}
								if (GMeta != nullptr){
									if(MetaSet.find(GMetaTy) == MetaSet.end()){
        		                        std::vector<TypeMeta> GtempTypeMetaV;
		                                GtempTypeMetaV = initTypeMetaV;
		                                MetaSet[GMetaTy] = GtempTypeMetaV;
									}
									std::vector<TypeMeta> tempTypeMetaV = std::vector<TypeMeta>();
	                                struct TypeMeta tempTypeMeta;
    	                            std::vector<StringRef> tempScope = std::vector<StringRef>();
        	                        std::vector<StringRef> tempVariables = std::vector<StringRef>();
                	                int tempModifier = 0;
                    	            tempTypeMetaV = MetaSet[GMetaTy];
                                	tempScope = tempTypeMetaV[0].scope;
									if(std::find(tempScope.begin(), tempScope.end(), GMetaScope) == tempScope.end()){
                                    	tempScope.push_back(GMetaScope);
                                	}
                                	tempVariables = tempTypeMetaV[0].variables;
									if(std::find(tempVariables.begin(), tempVariables.end(), GV->getName()) == tempVariables.end()){
										tempVariables.push_back(GV->getName());
									}
	                                tempModifier = mod;
    	                            tempTypeMetaV[0].scope = tempScope;
        	                        tempTypeMetaV[0].variables = tempVariables;
            	                    tempTypeMetaV[0].modifier = mod;
                	                mod++;
                    	            tempTypeMetaV[0].variables = tempVariables;
                        	        MetaSet[GMetaTy] = tempTypeMetaV;
								}
							}
							globalsDone = true;
						}
					}

					if (CallInst *CI = dyn_cast<CallInst>(I)){
						Function *CF = CI->getCalledFunction();
						if(CF && isLLVMMetaFunction(CF->getName())){
							if(Metadata *MetaCVTy = dyn_cast<MetadataAsValue>(I->getOperand(0))->getMetadata()){
								if(DIArgList *MetaDIArg = dyn_cast<DIArgList>(MetaCVTy)){
									continue;
								}
								if(!isa<ValueAsMetadata>(MetaCVTy)){
									continue;
								}
								if(Value* VTy = dyn_cast<ValueAsMetadata>(MetaCVTy)->getValue()){
									//Type* MetaType = VTy->getType()->getPointerElementType(); // For -O0
									Type* MetaType = VTy->getType(); // For -O2
									MetaTy = MetaType;
									if(!MetaType->isPointerTy()){
										continue;
									}
								}
							}						

							Metadata *MetaCV = cast<MetadataAsValue>(I->getOperand(1))->getMetadata();
							DILocalVariable *DIMetaCV = cast<DILocalVariable>(MetaCV);

							//PERMISSION
							if(Metadata *MetaCI = dyn_cast<MetadataAsValue>(I->getOperand(1))->getMetadata()){
								MDNode *MetaCINode = cast<MDNode>(MetaCI);
								if(DIDerivedType *MetaDTypeNode = dyn_cast<DIDerivedType>(MetaCINode->getOperand(3))){
									if(MetaDTypeNode->getBaseType() != nullptr){
										if(DIDerivedType *MetaDDTypeNode = dyn_cast<DIDerivedType>(MetaDTypeNode->getBaseType())){
											if(MetaDDTypeNode->getTag() == dwarf::DW_TAG_const_type){
												permission = 1;
											}
										}
									}
								}
							}
							// Detect if variable is escaping
							Metadata *MetaEsc = cast<MetadataAsValue>(I->getOperand(0))->getMetadata();
							if(DIArgList *MetaDIArg = dyn_cast<DIArgList>(MetaEsc)){
   	                            continue;
                            }
							Value* VEsc = cast<ValueAsMetadata>(MetaEsc)->getValue();
							bool isNonEscape = isNonEscapingLocalObject(VEsc, &IsCapturedCache);
								
							if(MetaSet.find(MetaTy) == MetaSet.end()){
								std::vector<TypeMeta> tempTypeMetaV;
								tempTypeMetaV = initTypeMetaV;
								MetaSet[MetaTy] = tempTypeMetaV;
							}

							std::vector<TypeMeta> TypeMetaV = std::vector<TypeMeta>(); 
							TypeMetaV = MetaSet[MetaTy];

							//ADD TO METADATA
							//CASE 0 - GLOBAL
							if (TypeMetaV[0].scope.empty() && permission == 0 && isNonEscape == 0){
								std::vector<TypeMeta> tempTypeMetaV = std::vector<TypeMeta>();
								struct TypeMeta tempTypeMeta;
								std::vector<StringRef> tempScope = std::vector<StringRef>();
								std::vector<StringRef> tempVariables = std::vector<StringRef>();
								int tempPermission = 0;
								int tempModifier = 0;

								tempTypeMetaV = MetaSet[MetaTy];
								tempScope = tempTypeMetaV[0].scope;
								tempVariables = tempTypeMetaV[0].variables;
								if(std::find(tempVariables.begin(), tempVariables.end(), DIMetaCV->getName()) == tempVariables.end()){
									tempVariables.push_back(DIMetaCV->getName());
								}
								if(std::find(tempScope.begin(), tempScope.end(), MetaScope) == tempScope.end()){
									tempScope.push_back(MetaScope);
								}
								tempPermission = permission;
								tempModifier = mod;
								tempTypeMetaV[0].scope = tempScope;
								tempTypeMetaV[0].variables = tempVariables;
								tempTypeMetaV[0].modifier = mod;
								mod++;
								tempTypeMetaV[0].variables = tempVariables;
								MetaSet[MetaTy] = tempTypeMetaV;
							}
							else if (permission == 0 && isNonEscape == 0){
								std::vector<StringRef> tempScope = std::vector<StringRef>();
								std::vector<StringRef> tempVariables = std::vector<StringRef>();
								tempScope = TypeMetaV[0].scope;
								tempVariables = TypeMetaV[0].variables;
								if(std::find(tempVariables.begin(), tempVariables.end(), DIMetaCV->getName()) == tempVariables.end()){
									tempVariables.push_back(DIMetaCV->getName());
								}
								if(std::find(tempScope.begin(), tempScope.end(), MetaScope) == tempScope.end()){
									tempScope.push_back(MetaScope);
								}
								TypeMetaV[0].scope = tempScope;
								TypeMetaV[0].variables = tempVariables;
								MetaSet[MetaTy] = TypeMetaV;
							}

							//CASE 1 - CONSTANT
							if(TypeMetaV[1].scope.empty() && permission == 1){
								std::vector<TypeMeta> tempTypeMetaV = std::vector<TypeMeta>();
								struct TypeMeta tempTypeMeta;
								std::vector<StringRef> tempScope = std::vector<StringRef>();
								std::vector<StringRef> tempVariables = std::vector<StringRef>();
								int tempPermission = 0;
								int tempModifier = 0;

								tempTypeMetaV = MetaSet[MetaTy];
								tempScope = tempTypeMetaV[1].scope;
								tempVariables = tempTypeMetaV[1].variables;
								if(std::find(tempVariables.begin(), tempVariables.end(), DIMetaCV->getName()) == tempVariables.end()){
                                	tempVariables.push_back(DIMetaCV->getName());
								}
								if(std::find(tempScope.begin(), tempScope.end(), MetaScope) == tempScope.end()){
									tempScope.push_back(MetaScope);
								}
								tempPermission = permission;
								tempModifier = mod;
								tempTypeMetaV[1].scope = tempScope;
								tempTypeMetaV[1].permission = permission;
								tempTypeMetaV[1].modifier = mod;
								mod++;
								tempTypeMetaV[1].variables = tempVariables;
								MetaSet[MetaTy] = tempTypeMetaV;
							}
							else if (permission == 1){
								std::vector<StringRef> tempScope = std::vector<StringRef>();
								std::vector<StringRef> tempVariables = std::vector<StringRef>();
								tempScope = TypeMetaV[1].scope;
								tempVariables = TypeMetaV[1].variables;
								if(std::find(tempVariables.begin(), tempVariables.end(), DIMetaCV->getName()) == tempVariables.end()){
                                	tempVariables.push_back(DIMetaCV->getName());
								}
								if(std::find(tempScope.begin(), tempScope.end(), MetaScope) == tempScope.end()){
                                       tempScope.push_back(MetaScope);
                                }
								TypeMetaV[1].scope = tempScope;
								TypeMetaV[1].variables = tempVariables;
								MetaSet[MetaTy] = TypeMetaV;
							}
							
							//CASE 3 - Escaping Local object
							if(TypeMetaV[2].scope.empty() && permission == 0 && isNonEscape == 1){
								std::vector<TypeMeta> tempTypeMetaV2 = std::vector<TypeMeta>();
								struct TypeMeta tempTypeMeta2;
								std::vector<StringRef> tempScope2 = std::vector<StringRef>();
								std::vector<StringRef> tempVariables2 = std::vector<StringRef>();
								int tempPermission2 = 0;
								int tempModifier2 = 0;

								tempTypeMetaV2 = MetaSet[MetaTy];
								tempScope2 = tempTypeMetaV2[2].scope;
								tempVariables2 = tempTypeMetaV2[2].variables;
								if(std::find(tempVariables2.begin(), tempVariables2.end(), DIMetaCV->getName()) == tempVariables2.end()){
                                	tempVariables2.push_back(DIMetaCV->getName());
								}
								if(std::find(tempScope2.begin(), tempScope2.end(), MetaScope) == tempScope2.end()){
									tempScope2.push_back(MetaScope);
								}
								tempPermission2 = permission;
								tempModifier2 = mod;
								tempTypeMetaV2[2].scope = tempScope2;
								tempTypeMetaV2[2].permission = permission;
								tempTypeMetaV2[2].modifier = mod;
								mod++;
								tempTypeMetaV2[2].variables = tempVariables2;
								MetaSet[MetaTy] = tempTypeMetaV2;
							}
							else if (permission == 0 && isNonEscape == 1){
								std::vector<StringRef> tempScope2 = std::vector<StringRef>();
								std::vector<StringRef> tempVariables2 = std::vector<StringRef>();
								tempScope2 = TypeMetaV[2].scope;
								tempVariables2 = TypeMetaV[2].variables;
								if(std::find(tempVariables2.begin(), tempVariables2.end(), DIMetaCV->getName()) == tempVariables2.end()){
                                	tempVariables2.push_back(DIMetaCV->getName());
								}
								if(std::find(tempScope2.begin(), tempScope2.end(), MetaScope) == tempScope2.end()){
         	                       tempScope2.push_back(MetaScope);
                                }
								TypeMetaV[2].scope = tempScope2;
								TypeMetaV[2].variables = tempVariables2;
								MetaSet[MetaTy] = TypeMetaV;
							}
						}
					}

					MDNode* N = MDNode::get(C, MDString::get(C, MetaScope));
					MDNode* N1 = MDNode::get(C, ConstantAsMetadata::get(ConstantInt::get(C, llvm::APInt(64, permission, false))));
					NN = M->getOrInsertNamedMetadata("my.md.name");
					NN->addOperand(N);
					NN->addOperand(N1);
				}
			}
		}
		}

		void instrumentPACIntrinsic(Function &F){
			if (isLibPAC(F.getName())){
				return;
			}
			for (Function::iterator FIt = F.begin(); FIt != F.end(); ++FIt){
				BasicBlock *BB = &*FIt;
				for (BasicBlock::iterator BBIt = BB->begin(); BBIt != BB->end(); ++BBIt){
					Instruction *I = &*BBIt;

					if (isa<PHINode>(I) || isa<LandingPadInst>(I)){
						continue;
					}
					LLVMContext &C = I->getFunction()->getContext();
					DataLayout DL = DataLayout(I->getModule());
					Module* M = I->getModule();
					std::vector<Type *> printfArgsTypes({llvm::Type::getInt8PtrTy(C)});
					FunctionType *printfType = FunctionType::get(llvm::Type::getInt32Ty(C), printfArgsTypes, true);
					FunctionCallee printfFunc = M->getOrInsertFunction("printf", printfType);
				
					if (StoreInst* SI = dyn_cast<StoreInst>(I)){
						Type* Ty = SI->getValueOperand()->getType();
						if (I->getPrevNode() != nullptr){
							if(IntToPtrInst* PTI = dyn_cast<IntToPtrInst>(I->getPrevNode())){
								continue;
							}
						}
						if (isLibPAC(F.getName())){
							return;
						}
						if (Ty->isPointerTy()){
							Type* RTy = getActualType(Ty);

							MDNode* modMeta = I->getMetadata("mod");
							Constant* modVal = dyn_cast<ConstantAsMetadata>(modMeta->getOperand(0))->getValue();
							int mod = cast<ConstantInt>(modVal)->getSExtValue();

							Value* pointer = SI->getValueOperand();

							IRBuilder<> builder(C);
							Value* key = llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 2);
							Value* modifier = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), mod);
							Value* printfPAC = builder.CreateGlobalStringPtr("PACDA: %p\n", "printfPAC", 0, M);
													
							Value* printfStore = builder.CreateGlobalStringPtr("PAC STORE: %p\n", "printfStore", 0, M);

							builder.SetInsertPoint(I->getNextNode());
							pointer = builder.CreatePtrToInt(pointer, builder.getIntPtrTy(DL), "auth.pi");

							StringRef funcNamePrint = F.getName();
							Value* funcNamePrintV = builder.CreateGlobalString(funcNamePrint);
							StringRef BBNamePrint = BB->getName();
							Value* BBNamePrintV = builder.CreateGlobalString(BBNamePrint);

							ArrayRef<Value*> argsStoreV({printfStore, pointer});
							Value* RHS = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 140737488355328);
							Value* result = builder.CreateICmpULE(pointer, RHS);
#ifdef STC_PAC
							Instruction *SPL = nullptr;
							if(Instruction* inst_result = dyn_cast<Instruction>(result)){
								 SPL = SplitBlockAndInsertIfThen(result, inst_result->getNextNode(), false);
							}
							else{
								SPL = SplitBlockAndInsertIfThen(result, I->getNextNode(), false);
							}
							builder.SetInsertPoint(SPL);
						
							ArrayRef<Type*> FuncParams = {
								Type::getInt64Ty(C),
								Type::getInt32Ty(C),
								Type::getInt64Ty(C)
							};

							Type* ResultFTy = Type::getInt64Ty(C);
							FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);
							FunctionCallee Func = I->getModule()->getOrInsertFunction("llvm.ptrauth.sign.i64", FTy);
							ArrayRef<Value*> FuncArgs = {
								pointer,
								key,
								modifier
							};

							pointer = builder.CreateCall(Func, FuncArgs);
												
							ArrayRef<Value*> argsV({printfPAC, pointer});

							pointer = builder.CreateIntToPtr(pointer, Ty);
							Instruction* SINew = SI->clone();
							SINew->setOperand(0, pointer);
							builder.Insert(SINew);
#endif
#ifdef STC_EOR
                            StringRef asmString = "eor x18, x18, x18\n";
                            StringRef constraints = "";
                            FunctionType* EFTy = FunctionType::get(Type::getVoidTy(C), false);

                            InlineAsm *IA = InlineAsm::get(EFTy, asmString, constraints, false);
                            ArrayRef<Value *> Args = None;
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
#endif
						}
					}
					
				
					else if (LoadInst* LI = dyn_cast<LoadInst>(I)){
						Type* Ty = LI->getType();
						StringRef LIName = LI->getName();
						if (isLibPAC(F.getName())){
							return;
						}
						
						if (Ty->isPointerTy()){
							Type* RTy = getActualType(Ty);
							if(BitCastInst *BCI = dyn_cast<BitCastInst>(LI->getPointerOperand())){
								Type* srcTy = BCI->getSrcTy();
								Type* destTy = BCI->getDestTy();
								if(srcTy->isPointerTy() && destTy->isPointerTy()){
									Type* srcTy2 = srcTy->getPointerElementType();
									Type* destTy2 = destTy->getPointerElementType();
									if(srcTy2->isPointerTy() && destTy2->isPointerTy()){
										if(P2PTypes.find(srcTy) != P2PTypes.end()){
											auto P2PTypes2 = P2PTypes.find(srcTy);
											unsigned int tag = P2PTypes2->second;
											insertLibCallBefore(LI->getPointerOperand(), I, AUTHPAC, tag);
											continue;
										}
									}
								}
							}


							MDNode* modMeta = I->getMetadata("mod");
							Constant* modVal = dyn_cast<ConstantAsMetadata>(modMeta->getOperand(0))->getValue();
							int mod = cast<ConstantInt>(modVal)->getSExtValue();
						
							Value* pointer = LI;

							IRBuilder<> builder(C);
							Value* key = llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 2);
    	                    Value* modifier = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), mod);
							Value* printfAuth = builder.CreateGlobalStringPtr("AUTDA: %p  %d  %s  %s\n", "printfAuth", 0, M);

							Value* printfLoad = builder.CreateGlobalStringPtr("LOAD: %p  %s  %s  %s\n", "printfLoad", 0, M);
							
							builder.SetInsertPoint(I->getNextNode());
							pointer = builder.CreatePtrToInt(pointer, builder.getIntPtrTy(DL), "auth.pi");

							StringRef funcNamePrint = F.getName();
                            Value* funcNamePrintV = builder.CreateGlobalString(funcNamePrint);
							StringRef BBNamePrint = BB->getName();
							Value* BBNamePrintV = builder.CreateGlobalString(BBNamePrint);

							ArrayRef<Value*> argsLoadV({printfLoad, pointer, funcNamePrintV, BBNamePrintV});
							Value* RHS = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 140737488355328);
							Value* result = builder.CreateICmpUGE(pointer, RHS);
							Instruction* inst_result = cast<Instruction>(result);
#ifdef STC_PAC
							Instruction* SPL = SplitBlockAndInsertIfThen(result, inst_result->getNextNode(), false);

							builder.SetInsertPoint(SPL);

                            ArrayRef<Type*> FuncParams = {
                                Type::getInt64Ty(C),
                                Type::getInt32Ty(C),
                                Type::getInt64Ty(C)
                            };

                            Type* ResultFTy = Type::getInt64Ty(C);
                            FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);
                            FunctionCallee Func = I->getModule()->getOrInsertFunction("llvm.ptrauth.auth.i64", FTy);
                            ArrayRef<Value*> FuncArgs = {
                                pointer,
                                key,
                                modifier
                            };
                            pointer = builder.CreateCall(Func, FuncArgs);
							
							ArrayRef<Value*> argsV({printfAuth, pointer, modifier, funcNamePrintV, BBNamePrintV});
							pointer = builder.CreateIntToPtr(pointer, Ty);
							
							builder.SetInsertPoint(SPL->getParent()->getSingleSuccessor()->getFirstNonPHI());
							PHINode* PN = builder.CreatePHI(Ty, 2, "auth.pn");
							PN->addIncoming(LI, LI->getParent());
							PN->addIncoming(pointer, SPL->getParent());
							
							LI->replaceUsesWithIf(PN, [](Use &U){
								if(PtrToIntInst* AuthPI = dyn_cast<PtrToIntInst>(U.getUser()))
								{
									StringRef AuthPIS = AuthPI->getName();
									if (AuthPIS.contains("auth")){
										return false;
									}
									else {
										return true;
									}
								}
								else if (PHINode* AuthPN = dyn_cast<PHINode>(U.getUser()))
								{	
									StringRef AuthPS = AuthPN->getName();
									if (AuthPS.contains("auth")){
										return false;
									}
									else {
										return true;
									}
								}
								return true;
							});
							
#endif
#ifdef STC_EOR
                            StringRef asmString = "eor x18, x18, x18\n";
                            StringRef constraints = "";
                            FunctionType* EFTy = FunctionType::get(Type::getVoidTy(C), false);

                            InlineAsm *IA = InlineAsm::get(EFTy, asmString, constraints, false);
                            ArrayRef<Value *> Args = None;
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
#endif
						}
					}										
				}
			}
		}
		
		void handlePointerToPointer(Module &M){
			for (auto &F :M){
				if (isLibPAC(F.getName())){
					continue;
				}
				for (Function::iterator FIt = F.begin(); FIt != F.end(); ++FIt){
					BasicBlock *BB = &*FIt;
					for (BasicBlock::iterator BBIt = BB->begin(); BBIt != BB->end(); ++BBIt){
						Instruction *I = &*BBIt;
						if (isa<PHINode>(I) || isa<LandingPadInst>(I)){
							continue;
						}
	
						if (CallInst* CI = dyn_cast<CallInst>(I)){
							if (CI->isTailCall()){
								continue;
							}
							Function* CF = CI->getCalledFunction();
							if (CF && !CI->arg_empty()){
								if (isLLVMFunction(CF->getName())){
									continue;
								}
								if (isExclude(CF->getName())){
									continue;
								}
								
								for (User::op_iterator ArgIt = CI->arg_begin(); ArgIt != CI->arg_end(); ++ArgIt){
									Value* arg = cast<Value>(ArgIt);
									Type* arg_type = arg->getType();
									if (arg_type->isPointerTy()){									
										Type* arg_type2 = arg_type->getPointerElementType();
										if (arg_type2->isPointerTy()){
											if (BitCastInst *BCI = dyn_cast<BitCastInst>(arg)){
												Type* srcTy = BCI->getSrcTy();
												Type* destTy = BCI->getDestTy();
			
												if (MetaSet.find(srcTy) != MetaSet.end()){
													if (MetaSet.find(destTy) != MetaSet.end()){
														if(P2PTypes.find(destTy) == P2PTypes.end()){
															P2PTypes[destTy] = type_id;
															insertSetMetaLibCall(type_id, I, SETMETA, tbi_tag, mod);
															insertLibCallBefore(arg, I, ADDPAC, tbi_tag);
															insertLibCallBefore(arg, I, ADDCETBI, tbi_tag);
															type_id++;
															tbi_tag++;
															mod++;
															if(tbi_tag == 250){
																outs() << "REACHED LIMIT FOR TBI\n";
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		void instrumentPACIntrinsicSTL(Function &F){
			if (isLibPAC(F.getName())){
					return;
			}
			for (Function::iterator FIt = F.begin(); FIt != F.end(); ++FIt){
				BasicBlock *BB = &*FIt;
				for (BasicBlock::iterator BBIt = BB->begin(); BBIt != BB->end(); ++BBIt){
					Instruction *I = &*BBIt;

					if (isa<PHINode>(I) || isa<LandingPadInst>(I)){
						continue;
					}
					LLVMContext &C = I->getFunction()->getContext();
					DataLayout DL = DataLayout(I->getModule());
					Module* M = I->getModule();
					std::vector<Type *> printfArgsTypes({llvm::Type::getInt8PtrTy(C)});
					FunctionType *printfType = FunctionType::get(llvm::Type::getInt32Ty(C), printfArgsTypes, true);
					FunctionCallee printfFunc = M->getOrInsertFunction("printf", printfType);

					if (AllocaInst* AI = dyn_cast<AllocaInst>(I)){
						Type *Ty = AI->getAllocatedType();
                        Value* pointer = &*AI;
						int mod;
						if (Ty->isPointerTy()){
							IRBuilder<> builder(C);
                            Value* key = llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 2);
							if(MetaSet.find(Ty) != MetaSet.end()){
								auto modMeta = MetaSet.find(Ty);
								mod = modMeta->second[0].modifier;
							}
							else {
								mod = 24;
							}
							Value* modifier = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), mod);
							builder.SetInsertPoint(I->getNextNode());
							Value* ampP = builder.CreatePointerCast(pointer, Ty->getPointerTo());
                            ampP = builder.CreatePtrToInt(ampP, builder.getIntPtrTy(DL));
                            modifier = builder.CreateXor(ampP, mod);
                            pointer = builder.CreatePtrToInt(pointer, builder.getIntPtrTy(DL));
							Value* RHS = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 140737488355328);
                            Value* result = builder.CreateICmpULE(pointer, RHS);
#ifdef STL_PAC
                            Instruction *SPL = nullptr;
                            if(Instruction* inst_result = dyn_cast<Instruction>(result)){
                                 SPL = SplitBlockAndInsertIfThen(result, inst_result->getNextNode(), false);
                            }
                            else{
                                SPL = SplitBlockAndInsertIfThen(result, I->getNextNode(), false);
                            }
							builder.SetInsertPoint(SPL);

                            ArrayRef<Type*> FuncParams = {
                                Type::getInt64Ty(C),
                                Type::getInt32Ty(C),
                                Type::getInt64Ty(C)
                            };

                            Type* ResultFTy = Type::getInt64Ty(C);
                            FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);
                            FunctionCallee Func = I->getModule()->getOrInsertFunction("llvm.ptrauth.sign.i64", FTy);
                            ArrayRef<Value*> FuncArgs = {
                                pointer,
                                key,
                                modifier
                            };

                            pointer = builder.CreateCall(Func, FuncArgs);
							pointer = builder.CreateIntToPtr(pointer, Ty);
#endif
#ifdef STL_EOR
							StringRef asmString = "eor x18, x18, x18\n";
                            StringRef constraints = "";
                            FunctionType* EFTy = FunctionType::get(Type::getVoidTy(C), false);

                            InlineAsm *IA = InlineAsm::get(EFTy, asmString, constraints, false);
                            ArrayRef<Value *> Args = None;
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
#endif							
						}
					}
					else if (StoreInst* SI = dyn_cast<StoreInst>(I)){
						Type* Ty = SI->getValueOperand()->getType();
						if (I->getPrevNode() != nullptr){
							if(IntToPtrInst* PTI = dyn_cast<IntToPtrInst>(I->getPrevNode())){
								continue;
							}
						}
						if (Ty->isPointerTy()){
							MDNode* modMeta = I->getMetadata("mod");
							Constant* modVal = dyn_cast<ConstantAsMetadata>(modMeta->getOperand(0))->getValue();
							int mod = cast<ConstantInt>(modVal)->getSExtValue();

							Value* pointer = SI->getValueOperand();

							IRBuilder<> builder(C);
							Value* key = llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 2);
							Value* modifier = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), mod);
							Value* printfPAC = builder.CreateGlobalStringPtr("PACDA: %p  %d  %s  %s\n", "printfPAC", 0, M);

							builder.SetInsertPoint(I->getNextNode());
							Value* ampP = builder.CreatePointerCast(pointer, Ty->getPointerTo());
							ampP = builder.CreatePtrToInt(ampP, builder.getIntPtrTy(DL));
                            modifier = builder.CreateXor(ampP, mod);
							pointer = builder.CreatePtrToInt(pointer, builder.getIntPtrTy(DL));

							Value* RHS = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 140737488355328);
							Value* result = builder.CreateICmpULE(pointer, RHS);
#ifdef STL_PAC							
							Instruction *SPL = nullptr;
							if(Instruction* inst_result = dyn_cast<Instruction>(result)){
								 SPL = SplitBlockAndInsertIfThen(result, inst_result->getNextNode(), false);
							}
							else{
								SPL = SplitBlockAndInsertIfThen(result, I->getNextNode(), false);
							}
							builder.SetInsertPoint(SPL);
						
							ArrayRef<Type*> FuncParams = {
								Type::getInt64Ty(C),
								Type::getInt32Ty(C),
								Type::getInt64Ty(C)
							};

							Type* ResultFTy = Type::getInt64Ty(C);
							FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);
							FunctionCallee Func = I->getModule()->getOrInsertFunction("llvm.ptrauth.sign.i64", FTy);
							ArrayRef<Value*> FuncArgs = {
								pointer,
								key,
								modifier
							};

							pointer = builder.CreateCall(Func, FuncArgs);
							
							pointer = builder.CreateIntToPtr(pointer, Ty);
							Instruction* SINew = SI->clone();
							SINew->setOperand(0, pointer);
							builder.Insert(SINew);
#endif
#ifdef STL_EOR							
							StringRef asmString = "eor x18, x18, x18\n";
                            StringRef constraints = "";
                            FunctionType* EFTy = FunctionType::get(Type::getVoidTy(C), false);

                            InlineAsm *IA = InlineAsm::get(EFTy, asmString, constraints, false);
                            ArrayRef<Value *> Args = None;
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
#endif							
						}
					}
					else if (LoadInst* LI = dyn_cast<LoadInst>(I)){
						Type* Ty = LI->getType();
						if (Ty->isPointerTy()){
							if(BitCastInst *BCI = dyn_cast<BitCastInst>(LI->getPointerOperand())){
								Type* srcTy = BCI->getSrcTy();
								Type* destTy = BCI->getDestTy();

								if(srcTy->isPointerTy() && destTy->isPointerTy()){
									Type* srcTy2 = srcTy->getPointerElementType();
									Type* destTy2 = destTy->getPointerElementType();
									if(srcTy2->isPointerTy() && destTy2->isPointerTy()){
										if(P2PTypes.find(srcTy) != P2PTypes.end()){
											auto P2PTypes2 = P2PTypes.find(srcTy);
											unsigned int tag = P2PTypes2->second;
											insertLibCallBefore(LI->getPointerOperand(), I, AUTHPAC, tag);
											continue;
										}
									}
								}
							}


							MDNode* modMeta = I->getMetadata("mod");
							Constant* modVal = dyn_cast<ConstantAsMetadata>(modMeta->getOperand(0))->getValue();
							int mod = cast<ConstantInt>(modVal)->getSExtValue();
							Value* pointer = LI;

							IRBuilder<> builder(C);
							Value* key = llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 2);
    	                    Value* modifier = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), mod);
							Value* printfAuth = builder.CreateGlobalStringPtr("AUTDA: %p  %d  %s  %s\n", "printfAuth", 0, M);


							builder.SetInsertPoint(I->getNextNode());
							Value* ampP = builder.CreatePointerCast(pointer, Ty->getPointerTo());
							ampP = builder.CreatePtrToInt(ampP, builder.getIntPtrTy(DL));
							modifier = builder.CreateXor(ampP, mod);
							pointer = builder.CreatePtrToInt(pointer, builder.getIntPtrTy(DL));

							Value* RHS = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 140737488355328);
							Value* result = builder.CreateICmpUGE(pointer, RHS);
							Instruction* inst_result = cast<Instruction>(result);
#ifdef STL_PAC
							Instruction* SPL = SplitBlockAndInsertIfThen(result, inst_result->getNextNode(), false);

							builder.SetInsertPoint(SPL);

                            ArrayRef<Type*> FuncParams = {
                                Type::getInt64Ty(C),
                                Type::getInt32Ty(C),
                                Type::getInt64Ty(C)
                            };

                            Type* ResultFTy = Type::getInt64Ty(C);
                            FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);
                            FunctionCallee Func = I->getModule()->getOrInsertFunction("llvm.ptrauth.auth.i64", FTy);
                            ArrayRef<Value*> FuncArgs = {
                                pointer,
                                key,
                                modifier
                            };
                            pointer = builder.CreateCall(Func, FuncArgs);

							pointer = builder.CreateIntToPtr(pointer, Ty);
							
							builder.SetInsertPoint(SPL->getParent()->getSingleSuccessor()->getFirstNonPHI());
							PHINode* PN = builder.CreatePHI(Ty, 2, "auth.pn");
							PN->addIncoming(LI, LI->getParent());
							PN->addIncoming(pointer, SPL->getParent());

							LI->replaceUsesWithIf(PN, [](Use &U){
								if(PtrToIntInst* AuthPI = dyn_cast<PtrToIntInst>(U.getUser()))
								{
									StringRef AuthPIS = AuthPI->getName();
									if (AuthPIS.contains("auth")){
										return false;
									}
									else {
										return true;
									}
								}
								else if (PHINode* AuthPN = dyn_cast<PHINode>(U.getUser()))
								{	
									StringRef AuthPS = AuthPN->getName();
									if (AuthPS.contains("auth")){
										return false;
									}
									else {
										return true;
									}
								}
								return true;
							});

#endif
#ifdef STL_EOR							
							StringRef asmString = "eor x18, x18, x18\n";
                            StringRef constraints = "";
                            FunctionType* EFTy = FunctionType::get(Type::getVoidTy(C), false);

                            InlineAsm *IA = InlineAsm::get(EFTy, asmString, constraints, false);
                            ArrayRef<Value *> Args = None;
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
                            builder.CreateCall(IA, Args);
#endif							
						}
					}
					else if (CallInst* CI = dyn_cast<CallInst>(I)){
						if (CI->isTailCall()){
							continue;
						}
						Function* CF = CI->getCalledFunction();
						if (CF && !CI->arg_empty()){
							if (isLLVMFunction(CF->getName())){
								continue;
							}
							if (isExclude(CF->getName())){
								continue;
							}
                            for (User::op_iterator ArgIt = CI->arg_begin(); ArgIt != CI->arg_end(); ++ArgIt){
								Value* pointer = cast<Value>(ArgIt);
                               	Type* Ty = pointer->getType();
                                if (Ty->isPointerTy()){
									IRBuilder<> builder(C);
        		                    Value* key = llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 2);
									if(MetaSet.find(Ty) != MetaSet.end()){
                             			auto modMeta = MetaSet.find(Ty);
		                                mod = modMeta->second[0].modifier;
        		                    }
                		            else {
                        		        mod = 24;
                            		}
		                            Value* modifier = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), mod);
									builder.SetInsertPoint(I->getNextNode());
                		            Value* ampP = builder.CreatePointerCast(pointer, Ty->getPointerTo());
        		                    ampP = builder.CreatePtrToInt(ampP, builder.getIntPtrTy(DL));
		                            modifier = builder.CreateXor(ampP, mod);
        		                    pointer = builder.CreatePtrToInt(pointer, builder.getIntPtrTy(DL));
                		            Value* RHS = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), 140737488355328);
        		                    Value* result = builder.CreateICmpUGE(pointer, RHS);
#ifdef STL_PAC
		                            if(Instruction* inst_result = dyn_cast<Instruction>(result))
									{
										Instruction* SPL = SplitBlockAndInsertIfThen(result, inst_result->getNextNode(), false);
										builder.SetInsertPoint(SPL);
		                            	ArrayRef<Type*> FuncParams = {
                            		    	Type::getInt64Ty(C),
	                    		            Type::getInt32Ty(C),
    	        		                    Type::getInt64Ty(C)
    			                        };
            	    		            Type* ResultFTy = Type::getInt64Ty(C);
        			                    FunctionType* FTy = FunctionType::get(ResultFTy, FuncParams, false);
		            	                FunctionCallee Func = I->getModule()->getOrInsertFunction("llvm.ptrauth.auth.i64", FTy);
                        			    ArrayRef<Value*> FuncArgs = {
                		    	            pointer,
        		                	        key,
		                            	    modifier
	                            		};
										pointer = builder.CreateCall(Func, FuncArgs);
										pointer = builder.CreateIntToPtr(pointer, Ty);
									}
#endif								
#ifdef STL_EOR							
									StringRef asmString = "eor x18, x18, x18\n";
        		                    StringRef constraints = "";
                		            FunctionType* EFTy = FunctionType::get(Type::getVoidTy(C), false);

                        		    InlineAsm *IA = InlineAsm::get(EFTy, asmString, constraints, false);
		                            ArrayRef<Value *> Args = None;
        		                    builder.CreateCall(IA, Args);
                		            builder.CreateCall(IA, Args);
                        		    builder.CreateCall(IA, Args);
		                            builder.CreateCall(IA, Args);
        		                    builder.CreateCall(IA, Args);
                		            builder.CreateCall(IA, Args);
                        		    builder.CreateCall(IA, Args);
#endif							

								}
							}
						}
					}
				}
			}
		}
			
		bool runOnModule (Module &M) override {		
			//LLVM Metadata insertion
			llvmMetadataFetch(M);
			
			//Handle pointer-to-pointer
			handlePointerToPointer(M);

			for (auto &F :M){
				if (isLLVMFunction(F.getName()) && !F.isDeclaration()){
					continue;
				}

				if (isLibPAC(F.getName())){
					continue;
				}
				//Handle modifiers for any bitcasts
//				handleBitcastsSTC(F);
				handleBitcastsSTWC(F);

				//Add modifier metadata to load/store
				insertModifierMeta(F);
				instrumentPACIntrinsic(F);
//				instrumentPACIntrinsicSTL(F);
			}
	
			outs() << "\n\n\n" << "<<<<<<<<<<<< METADATA <<<<<<<<<<<" << "\n";
			long varNum = 0;
			long typeNum = 0;
			struct ModifierCountSt{
				int count;
				int numVariables;
			};
			unordered_map<int, ModifierCountSt> ModifierCount;
			outs() << "Number of Types without cases: " << MetaSet.size() << "\n";
			for(int i = 0; i < 3; i++){
				outs() << "CASE: " << i << "\n";
				for(auto MetaSetIt = MetaSet.begin(); MetaSetIt != MetaSet.end(); ++MetaSetIt){
					outs() << "Type: ";
					std::string type_str;
					llvm::raw_string_ostream rso(type_str);
					MetaSetIt->first->print(rso);
					std::string print = rso.str();
					outs() << print << "\n";
					outs() << "Scope of use:\n ";
					for (StringRef i: MetaSetIt->second[i].scope){
						outs() << i << ", ";
					}
					outs() << "\n";
					outs() << "Variables:\n ";
					for (StringRef i: MetaSetIt->second[i].variables){
						outs() << i << ", ";
					}
					outs() << "\nNo. of variables: " << MetaSetIt->second[i].variables.size();
					varNum = varNum + MetaSetIt->second[i].variables.size();
					outs() << "\nPermission: " << MetaSetIt->second[i].permission << "\n\n";

					if(MetaSetIt->second[i].modifier != 0){
						typeNum = typeNum + 1;
					}

					if(ModifierCount.find(MetaSetIt->second[i].modifier) == ModifierCount.end()){
						if(MetaSetIt->second[i].modifier != 0){
							ModifierCount[MetaSetIt->second[i].modifier].count = 1;
							ModifierCount[MetaSetIt->second[i].modifier].numVariables = MetaSetIt->second[i].variables.size();
						}
					}
					else{
						ModifierCount[MetaSetIt->second[i].modifier].count = ModifierCount[MetaSetIt->second[i].modifier].count + 1;
						ModifierCount[MetaSetIt->second[i].modifier].numVariables = ModifierCount[MetaSetIt->second[i].modifier].numVariables + MetaSetIt->second[i].variables.size();
					}
				}
				outs() << "\n";
			}
			outs() << "Number of Types: " << typeNum << "\n";
			outs() << "\n\n";
			
			return true;
		}

		bool isLLVMFunction(StringRef s){
			return s.startswith("llvm.") || s.startswith("__llvm__");
		}

		bool isLLVMMetaFunction(StringRef s){
			return s.startswith("llvm.dbg.value") || s.startswith("llvm.dbg.declare");
		}

		bool isAllocFunction(StringRef s){
			return s.contains("alloc");
		}

		bool isFreeFunction(StringRef s){
			return s.contains("free");
		}

		bool isMemFunction(StringRef s){
			return s.contains("mmap") || s.contains("munmap") || s.contains("memcpy") || s.contains("memset") || s.contains("strcpy") || s.contains("memcmp") || s.contains("fread") || s.contains("strcat");
		}

		bool isExclude(StringRef s){
			return isAllocFunction(s) || isFreeFunction(s) || isMemFunction(s);
		}

		bool isLibPAC(StringRef s){
			return s.contains("addPACpp") || s.contains("authenticatePACpp") || s.contains("setMetadata") || s.contains("metadataExists") || s.contains("addCEtbi") || s.contains("init_memory") || s.contains("dvipac_set") || s.contains("dvipac_get");
		}
	};
}

char DVIPACInstr::ID = 0;
INITIALIZE_PASS(DVIPACInstr, // Name of pass class, e.g., MyAnalysis
        "dvi-pac", // Command-line argument to invoke pass
        "DVI PAC implementation", // Short description of pass
        false, // Does this pass modify the control-flow graph?
        false) // Is this an analysis pass?

namespace llvm {
    ModulePass *createDVIPACInstrPass() {
        return new DVIPACInstr();
    }
}

