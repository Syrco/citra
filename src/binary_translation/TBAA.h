#pragma once
#include "Instructions/Types.h"
#include <llvm/IR/Instructions.h>

namespace llvm
{
    class Instruction;
    class MDNode;
}

/*
Manages TBAA.
A TBAA type is generated for each register and global.
It is a bit of an abuse of TBAA but because nothing aliases it is a good way
to notify LLVM of it.
*/

class TBAA
{
public:
    void GenerateTags();

    void TagRegister(llvm::Instruction *instruction, Register reg);
    void TagConst(llvm::Instruction *instruction);
    void TagInstructionCount(llvm::Instruction *instruction);
    void TagMemory(llvm::Instruction *instruction);
private:
    llvm::MDNode *register_nodes[RegisterCount];
    // Tag for everything that is never written.
    // Since it is never written, one tag works
    llvm::MDNode *const_node;
    llvm::MDNode *instruction_count_node;
    llvm::MDNode *memory_node;
};