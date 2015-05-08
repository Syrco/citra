#include "Instruction.h"
#include "Types.h"

/*
* Data processing instructions
* ARMv7-A 5.2.1 (register), 5.2.2 (register-shifted register), 5.2.3 (immediate)
*/

class Arithmetic : public Instruction
{
public:
    enum class Form
    {
        Register, RegisterShiftedRegister, Immediate
    };
    enum class Op
    {
        BitwiseAnd = 0, BitwiseXor, Subtract, RevSubtract, Add, AddWithCarry, SubtractWithCarry, ReverseSubtractWithCarry,
        // Compare, Test, Misc
        BitwiseOr = 12, MoveAndShifts, BitwiseBitClear, BitwiseNot
    };

    bool Decode() override;
    void GenerateInstructionCode(InstructionBlock* instruction_block) override;
private:
    Form form;
    Op op;
    bool set_flags;
    Register rn;
    Register rd;
    u32 imm5;
    u32 imm12;
    u32 type;
    Register rm;
};