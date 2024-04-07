//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"


using namespace llvm;

Instruction* currentInstruction = nullptr;

bool runOnBasicBlock(BasicBlock &B) {
  for(BasicBlock::iterator BBIterator = B.begin(); BBIterator != B.end(); ++BBIterator){
    // Ad ogni iterazione aggiorno l'istruzione corrente.
    currentInstruction = &(*BBIterator);
    Value *operando0 = currentInstruction->getOperand(0);
    ConstantInt *CI0 = dyn_cast<ConstantInt>(operando0);
    Value *operando1 = currentInstruction->getOperand(1);
    ConstantInt *CI1 = dyn_cast<ConstantInt>(operando1);

    // controllo che l'istruzione sia una Mul
    if(currentInstruction->getOpcode() == Instruction::Mul){

      if( CI0 and CI0->getValue().isOne()){
        LocalOpts::algebraicIdentity(operando1);
      }
      if(CI1 and CI1->getValue().isOne()){
        LocalOpts::algebraicIdentity(operando0);
      }

      if(CI0 and CI0->getValue().isPowerOf2() and CI0->getValue()!=1){ // non controllo che sia positivo perche tanto se fosse negativo sicuramente non è una potenza di 2
        // L'operazione è una moltiplicazione e l'operando 0 è una potenza di 2
        LocalOpts::replaceMulwShift(operando1,CI0);
        continue;
      }else if(CI1 and CI1->getValue().isPowerOf2() and CI1->getValue()!=1){
        // L'operazione è una Mul, il primo operando non era un intero o comunque non era una potenza di 2 ma il secondo lo è
        LocalOpts::replaceMulwShift(operando0,CI1);
        continue;
      }

      if(CI0 and not CI1 and CI0->getValue().isStrictlyPositive()){
        LocalOpts::advancedMulwShift(CI0, operando1);
      }
      if(CI1 and not CI0 and CI1->getValue().isStrictlyPositive()){
        LocalOpts::advancedMulwShift(CI1, operando0);
      }
    }


    if(currentInstruction->getOpcode() == Instruction::Add || currentInstruction->getOpcode()==Instruction::Sub){

      if(CI0 and CI0->getValue().isZero())
        LocalOpts::algebraicIdentity(operando1);
      if(CI1 and CI1->getValue().isZero())
        LocalOpts::algebraicIdentity(operando0);

      if(CI0 and not CI1){LocalOpts::multiInstructionOptimization(CI0, 1);}
      if(CI1 and not CI0){LocalOpts::multiInstructionOptimization(CI1, 0);}

    }
    if(currentInstruction->getOpcode() == Instruction::SDiv){
      if(CI0 and not CI1 and CI0->getValue().isStrictlyPositive()){
        LocalOpts::advancedDivwShift(CI0, operando1);
      }
      if(CI1 and not CI0 and CI1->getValue().isStrictlyPositive()){
        LocalOpts::advancedDivwShift(CI1, operando0);
      }

    }

  }
  return true;
}

bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
  }

  return Transformed;
}

void LocalOpts::multiInstructionOptimization(ConstantInt* costante, int posOperando){

  APInt valoreCostante = costante->getValue();
  APInt userConst;

  Instruction::BinaryOps istruzioneOpposta = Instruction::Add;
  if(currentInstruction->getOpcode()==Instruction::Add)
    istruzioneOpposta = Instruction::Sub;


  for(auto userIter = currentInstruction->user_begin(); userIter != currentInstruction->user_end(); ++userIter){
    Instruction* userInstruction = dyn_cast<Instruction>(*userIter);
    // se non è una sub o una add, passo alla prossima istruzione
    if(userInstruction->getOpcode() != istruzioneOpposta)
      continue;
    Value* userOperand0 = userInstruction->getOperand(0);
    ConstantInt* UCI0 = dyn_cast<ConstantInt>(userOperand0);
    if(not UCI0){
      Value* userOperand1 = userInstruction->getOperand(1);
      ConstantInt* UCI1 = dyn_cast<ConstantInt>(userOperand1);
      if(not UCI1)
        continue; // sono entrambi registri
      userConst = UCI1->getValue();
    }else{
      userConst = UCI0->getValue();
    }

    if(userConst.eq(valoreCostante)){
      userInstruction->replaceAllUsesWith(currentInstruction->getOperand(posOperando));
    }


  }
}

void LocalOpts::advancedMulwShift(ConstantInt* costanteMoltiplicativa, Value* registro){
  const APInt& costante = costanteMoltiplicativa->getValue();
  unsigned nearestLog2 = costante.nearestLogBase2();

  Instruction *ShiftArrotondato = BinaryOperator::Create(Instruction::Shl,
                                                         registro,
                                                         ConstantInt::get(costanteMoltiplicativa->getType(),
                                                         nearestLog2));
  // La inserisco io quindi per ora non la usa nessuno
  ShiftArrotondato->insertAfter(currentInstruction);
  // calcolo l'offset da sottrarre/aggiungere
  unsigned potenza2Successiva = 1 << nearestLog2;
  APInt offset = costante-potenza2Successiva;

  // Moltiplico l'offset con il registro.
  Instruction *offsetCorretto = BinaryOperator::Create(Instruction::Mul,
                                                       registro,
                                                       ConstantInt::get(costanteMoltiplicativa->getType(),
                                                       offset));

  offsetCorretto->insertAfter(ShiftArrotondato);
  // Concettualmente era corretto. Pero posso anche sempre aggiungere...
  // L'offset è già una quantità positiva o negativa quindi aggiungo questa quantità.
  // Se l'offset fosse stato unsigned allora avrei dovuto differenziare i due casi.
  // Questo sarà anche meno carino perché andando a leggere la IR sembra che aggiungo sempre ma magari sto aggiungendo una quantità negativa. Codice meno chiaro ma più compatto.
  /*
  if(offset.isStriclyPositive()){
    // Vuol dire che devo aggiungere. Lo shift arrotondato era per difetto
    BinaryOperator::Create(Instruction::Add, ShiftArrotondato, offsetCorretto, "", ShiftArrotondato->getNextNode());

  }else{
    // Vuol dire che devo sottrarre. Lo shift arrotondato era per eccesso
    BinaryOperator::Create(Instruction::Sub, ShiftArrotondato, offsetCorretto, "", ShiftArrotondato->getNextNode());
  }
  */

  Instruction* mulOptimized = BinaryOperator::Create(Instruction::Add,
                                                     ShiftArrotondato,
                                                     offsetCorretto,
                                                     "",
                                                     offsetCorretto->getNextNode());

  currentInstruction->replaceAllUsesWith(mulOptimized);

}

void LocalOpts::advancedDivwShift(ConstantInt* costanteDivisoria, Value* registro){

  const APInt& costante = costanteDivisoria->getValue();
  unsigned nearestLog2 = costante.nearestLogBase2();

  Instruction *ShiftArrotondato = BinaryOperator::Create(Instruction::LShr,
                                                         registro,
                                                         ConstantInt::get(costanteDivisoria->getType(),
                                                         nearestLog2));
  ShiftArrotondato->insertAfter(currentInstruction);
  unsigned potenza2Successiva = 1 << nearestLog2;
  APInt offset = costante-potenza2Successiva;
  Instruction *offsetCorretto = BinaryOperator::Create(Instruction::Mul,
                                                       registro,
                                                       ConstantInt::get(costanteDivisoria->getType(),
                                                       offset));
  offsetCorretto->insertAfter(ShiftArrotondato);
  Instruction* divOptimized = BinaryOperator::Create(Instruction::Add,
                                                     ShiftArrotondato,
                                                     offsetCorretto,
                                                     "",
                                                     offsetCorretto->getNextNode());

  currentInstruction->replaceAllUsesWith(divOptimized);
}

void LocalOpts::algebraicIdentity(Value* valueNotIdentity){
  currentInstruction->replaceAllUsesWith(valueNotIdentity);
}

void LocalOpts::replaceMulwShift(Value* moltiplicatore, ConstantInt* fattore){

  Instruction *shiftEquivalente = BinaryOperator::Create(Instruction::Shl,
                                                         moltiplicatore,
                                                         ConstantInt::get(fattore->getType(), fattore->getValue().logBase2()));

  shiftEquivalente->insertAfter(currentInstruction);
  currentInstruction->replaceAllUsesWith(shiftEquivalente);
}

PreservedAnalyses LocalOpts::run(Module &M,
                                      ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();

  return PreservedAnalyses::all();
}






