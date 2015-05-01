#include "Instruction.h"
#include "Types.h"

/*
 * Data processing instructions
 * ARMv7-A 5.2.1 (register), 5.2.2 (register-shifted register, 5.2.3 (immediate)
 */

class DataProcessing : public Instruction
{
public:
    /*
     * The 4 bit op types (1 = 0001x: BitwiseXor, etc...)
     */
    enum class ShortOpType
    {
        BitwiseAnd = 0, BitwiseXor, Subtract, RevSubtract, Add, AddWithCarry, SubtractWithCarry, ReverseSubtractWithCarry,
        // Compare, Test, Misc
        BitwiseOr = 12, MoveAndShifts, BitwiseBitClear, BitwiseNot
    };
    enum class Op2Type
    {
        MoveAndLSL, LSR, ASR, RRXAndROR
    };
    enum class Form
    {
        Register, RegisterShiftedRegister, Immediate
    };

public:
    virtual bool Decode() override;
    void GenerateInstructionCode(InstructionBlock* instruction_block) override;
private:
    Form form;
    ShortOpType short_op;
    bool s;
    Register rn;
    Register rd;
    Register rm;
    u32 imm12;
    u32 imm5;
    Op2Type op2;
};