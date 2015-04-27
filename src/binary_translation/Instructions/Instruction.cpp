#include "Instruction.h"
#include <cassert>

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
    return Decode();
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