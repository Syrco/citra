#include "TBAA.h"
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <sstream>
#include <llvm/IR/Instruction.h>

using namespace llvm;

void TBAA::GenerateTags()
{
    MDBuilder md_builder(getGlobalContext());

    auto tbaa_root = md_builder.createTBAARoot("Root");

    for (auto i = 0; i < RegisterCount; ++i)
    {
        std::stringstream ss;
        ss << "Register_" << i;
        register_nodes[i] = md_builder.createTBAAScalarTypeNode(ss.str(), tbaa_root);
    }
    const_node = md_builder.createTBAAScalarTypeNode("Readonly", tbaa_root);
    instruction_count_node = md_builder.createTBAAScalarTypeNode("InstructionCount", tbaa_root);
    memory_node = md_builder.createTBAAScalarTypeNode("Memory", tbaa_root);
}

void TBAA::TagRegister(Instruction* instruction, Register reg)
{
    instruction->setMetadata(LLVMContext::MD_tbaa, register_nodes[(int)reg]);
}

void TBAA::TagConst(Instruction* instruction)
{
    instruction->setMetadata(LLVMContext::MD_tbaa, const_node);
}

void TBAA::TagInstructionCount(llvm::Instruction* instruction)
{
    instruction->setMetadata(LLVMContext::MD_tbaa, instruction_count_node);
}

void TBAA::TagMemory(llvm::Instruction* instruction)
{
    instruction->setMetadata(LLVMContext::MD_tbaa, memory_node);
}