#pragma once
#include "Instructions/Types.h"

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
private:
    llvm::MDNode *register_nodes[RegisterCount];
    // Tag for everything that is never written.
    // Since it is never written, one tag works
    llvm::MDNode *const_node;
};