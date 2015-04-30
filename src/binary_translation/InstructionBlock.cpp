#include "InstructionBlock.h"
#include "ModuleGen.h"
#include "Instructions/Instruction.h"
#include <sstream>
#include <iomanip>
#include "MachineState.h"

InstructionBlock::InstructionBlock(ModuleGen* module, Instruction* instruction)
    : module(module),
      instruction(std::unique_ptr<Instruction>(instruction))
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << instruction->Address();
    address_string = ss.str();
}

InstructionBlock::~InstructionBlock()
{
}

void InstructionBlock::GenerateEntryBlock()
{
    entry_basic_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), address_string + "_Entry");
}

void InstructionBlock::GenerateCode()
{
    auto ir_builder = Module()->IrBuilder();
    ir_builder->SetInsertPoint(entry_basic_block);

    instruction->GenerateCode(this);

    auto basic_block = ir_builder->GetInsertBlock();
    // If the basic block is terminated there has been a jump
    // If not, jump to the next instruction
    if (!basic_block->getTerminator())
    {
        Module()->WritePCConst(Address() + 4);
    }
}

llvm::Value *InstructionBlock::Read(Register reg)
{
    return module->Machine()->ReadRegiser(reg);
}

llvm::Value *InstructionBlock::Write(Register reg, llvm::Value *value)
{
    return module->Machine()->WriteRegiser(reg, value);
}

size_t InstructionBlock::Address()
{
    return instruction->Address();
}