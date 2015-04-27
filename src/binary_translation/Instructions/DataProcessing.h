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
        BitwiseOr = 12, Move, BitwiseBitClear, BitwiseNot
    };
    enum class Form
    {
        Register, RegisterShiftedRegister, Immediate
    };

public:
    virtual bool Decode() override;
private:
    Form form;
    Condition cond;
    ShortOpType short_op;
    bool s;
    Register rn;
    Register rd;
    u32 imm12;
};