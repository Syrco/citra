#include "Instruction.h"
#include "common/logging/log.h"
#include <cassert>
#include "InstructionBlock.h"
#include "ModuleGen.h"
#include "MachineState.h"
#include "BinarySearch.h"

Instruction::Instruction()
{
}

Instruction::~Instruction()
{
}

bool Instruction::Read(u32 instruction, u32 address)
{
    this->instruction = instruction;
    this->address = address;
    // Call the read of derived class
    if (!Decode()) return false;

    if (cond == Condition::Invalid) return false;
    return true;
}

void Instruction::GenerateCode(InstructionBlock *instruction_block)
{
    auto ir_builder = instruction_block->Module()->IrBuilder();

    if (cond == Condition::AL)
    {
        GenerateInstructionCode(instruction_block);
    }
    else
    {
        auto pred = instruction_block->Module()->Machine()->ConditionPassed(cond);
        auto passed_block = instruction_block->CreateBasicBlock("Passed");
        auto not_passed_block = instruction_block->CreateBasicBlock("NotPassed");

        ir_builder->CreateCondBr(pred, passed_block, not_passed_block);

        ir_builder->SetInsertPoint(passed_block);
        GenerateInstructionCode(instruction_block);
        // If the basic block is terminated there has been a jump
        // If not, jump to the next not passed block (which will jump to the next instruction)
        if (!ir_builder->GetInsertBlock()->getTerminator())
        {
            ir_builder->CreateBr(not_passed_block);
        }

        ir_builder->SetInsertPoint(not_passed_block);
    }
    // If the basic block is terminated there has been a jump
    // If not, jump to the next instruction
    if (!ir_builder->GetInsertBlock()->getTerminator())
    {
        instruction_block->Module()->BranchWritePCConst(Address() + 4);
    }
}

bool Instruction::ReadFields(const std::initializer_list<FieldDefObject> &fields)
{
    size_t total_bit_count = 0;
    auto current_instruction = instruction;

    for (auto &field : fields)
    {
        total_bit_count += field.BitCount();
        // Read the upper bits
        auto value = current_instruction >> (32 - field.BitCount());
        if (!field.Read(value)) return false;
        // Remove the upper bits
        current_instruction <<= field.BitCount();
    }
    assert(total_bit_count == 32);

    return true;
}

Instruction::FieldDefObject Instruction::CondDef()
{
    return FieldDef<4>(&cond);
}

Instruction::FieldDefObject::FieldDefObject(u32 bit_count, u32 const_value)
    : bit_count(bit_count), const_value(const_value), constant(true)
{
}

Instruction::FieldDefObject::FieldDefObject(u32 bit_count, void* field_address, WriteFunctionType write_function)
    : bit_count(bit_count), field_address(field_address), write_function(write_function), constant(false)
{
}

bool Instruction::FieldDefObject::Read(u32 value) const
{
    if (constant)
    {
        return value == const_value;
    }
    else
    {
        write_function(value, field_address);
        return true;
    }
}