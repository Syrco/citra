#include "common/common_types.h"
#include <memory>

class Instruction;

class Disassmbler
{
public:
    /*
     * Returns the instruction at address or null if unknown or not translatable
     * address is used for PC relative operations
     */
    static std::unique_ptr<Instruction> Disassemble(u32 instruction, u32 address);
};

class RegisterInstructionBase
{
public:
    typedef Instruction *(*CreateFunctionType)(u32 instruction, u32 address);

    RegisterInstructionBase(CreateFunctionType create_function);
};

/*
 * Instantiate this class in a source file to register instruction in the disassembler
 */
template<typename DerivedInstruction>
class RegisterInstruction : RegisterInstructionBase
{
public:
    RegisterInstruction() : RegisterInstructionBase(&RegisterInstruction::Create) {}
private:
    static Instruction *Create(u32 instruction, u32 address)
    {
        auto result = new DerivedInstruction(instruction, address);
        if (!result->Read())
        {
            delete result;
            return nullptr;
        }
        return result;
    }
};