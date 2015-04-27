#pragma once
#include "common/common_types.h"
#include <initializer_list>

class Instruction
{
protected:
    class FieldDefObject;
public:
    Instruction(u32 instruction, u32 address);
    virtual ~Instruction();

    /*
     * Reads the instruction.
     * Returns true on success, or false otherwise
     */
    virtual bool Read() = 0;
protected:
    /*
     * Reads fields from the instruction
     * The fields come most significant first
     */
    bool ReadFields(const std::initializer_list<FieldDefObject> &fields);

    /*
     * Creates a field definition for a constant
     */
    template<size_t BitCount>
    static FieldDefObject FieldDef(u32 value);
    /*
     * Creates a field definition for a field
     */
    template<size_t BitCount, typename Type>
    static FieldDefObject FieldDef(Type *field);

private:
    /*
     * Function used by FieldDefObject to write to a field
     */
    template<typename Type>
    static void WriteFunction(u32 value, void *field_address);

    // Instruction value
    u32 instruction;
    // Instruction address
    u32 address;
};

/*
 * Object produced by FieldDef
 */
class Instruction::FieldDefObject
{
public:
    typedef void(*WriteFunctionType)(u32 value, void *field_address);
public:
    // Constant
    FieldDefObject(u32 bit_count, u32 const_value);
    // Field
    FieldDefObject(u32 bit_count, void *field_address, WriteFunctionType write_function);
    bool Read(u32 value) const;
    u32 BitCount() const { return bit_count; }
private:
    u32 bit_count;
    u32 const_value;
    void *field_address;
    WriteFunctionType write_function;
    bool constant;
};

template <size_t BitCount>
Instruction::FieldDefObject Instruction::FieldDef(u32 value)
{
    return FieldDefObject(BitCount, value);
}

template <size_t BitCount, typename Type>
Instruction::FieldDefObject Instruction::FieldDef(Type* field)
{
    return FieldDefObject(BitCount, field, &Instruction::WriteFunction<Type>);
}

template <typename Type>
void Instruction::WriteFunction(u32 value, void *field_address)
{
    *static_cast<Type *>(field_address) = static_cast<Type>(value);
}