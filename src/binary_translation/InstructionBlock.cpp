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
    ss << std::hex << std::setfill('0') << std::setw(8) << instruction->Address() << "_";
    address_string = ss.str();
}

InstructionBlock::~InstructionBlock()
{
}

void InstructionBlock::GenerateEntryBlock()
{
    entry_basic_block = CreateBasicBlock("Entry");
}

void InstructionBlock::GenerateCode()
{
    auto ir_builder = Module()->IrBuilder();
    ir_builder->SetInsertPoint(entry_basic_block);

    module->GenerateIncInstructionCount();

    instruction->GenerateCode(this);
}

llvm::Value *InstructionBlock::Read(Register reg)
{
    return module->Machine()->ReadRegiser(reg);
}

llvm::Value *InstructionBlock::Write(Register reg, llvm::Value *value)
{
    return module->Machine()->WriteRegiser(reg, value);
}

llvm::BasicBlock *InstructionBlock::CreateBasicBlock(const char *name)
{
    return llvm::BasicBlock::Create(llvm::getGlobalContext(), address_string + name);
}

void InstructionBlock::Link(InstructionBlock* prev, InstructionBlock* next)
{
	prev->nexts.push_back(next);
	next->prevs.push_back(prev);
}

u32 InstructionBlock::Address()
{
    return instruction->Address();
}