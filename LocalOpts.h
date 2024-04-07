#ifndef LLVM_TRANSFORMS_LOCALOPTS_H
#define LLVM_TRANSFORMS_LOCALOPTS_H
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Constants.h"
namespace llvm {
    class LocalOpts : public PassInfoMixin<LocalOpts> {
        public:
            PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
            static void replaceMulwShift(Value* v, ConstantInt* CI);
            static void algebraicIdentity(Value* v);
            static void advancedMulwShift(ConstantInt* CI, Value* registro);
            static void advancedDivwShift(ConstantInt* CD, Value* registro);
            static void multiInstructionOptimization(ConstantInt* costante, int pos);
    };

} // namespace llvm
#endif // LLVM_TRANSFORMS_LOCALOPTS_H

