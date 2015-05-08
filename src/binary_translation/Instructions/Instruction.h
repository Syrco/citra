#pragma once
#include "common/common_types.h"
#include <initializer_list>
#include "Types.h"

class InstructionBlock;

class Instruction
{
protected:
    class FieldDefObject;
public:
    Instruction();
    virtual ~Instruction();

    /*
     * Reads the instruction.
     * Returns true on success, or false otherwise
     */
    bool Read(u32 instruction, u32 address);

    /*
     * Generates non instruction specific code, and then calls GenerateInstructionCode
     */
    void GenerateCode(InstructionBlock *instruction_block);

    u32 Address() const { return address; }
protected:
    /*
     * Derived classes must override this, and implement it by calling ReadFields
     */
    virtual bool Decode() = 0;
    /*
     * Generates code for the instruction into the instruction block
     * Derived classes must override this
     */
    virtual void GenerateInstructionCode(InstructionBlock *instruction_block) = 0;
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
    /*
    * Creates a field definition for the condition field
    */
    FieldDefObject CondDef();
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

    Condition cond;
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
    *static_cast<Type *>(field_address) = Type(value);
}