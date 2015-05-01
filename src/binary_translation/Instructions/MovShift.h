#include "Instruction.h"
#include "Types.h"

/*
 * Data processing instructions
 * ARMv7-A 5.2.1 (register), 5.2.2 (register-shifted register, 5.2.3 (immediate)
 */

class MovShift : public Instruction
{
public:
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
    bool s;
    Register rn;
    Register rd;
    Register rm;
    u32 imm12;
    u32 imm5;
    Op2Type op2;
};