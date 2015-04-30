#include "Instruction.h"
#include "Types.h"

/*
 * Data processing instructions
 * ARMv7-A 5.2.1 (register), 5.2.2 (register-shifted register, 5.2.3 (immediate)
 */

class Branch : public Instruction
{
public:
    enum class Form
    {
        Immediate, Register
    };

public:
    virtual bool Decode() override;
    void GenerateInstructionCode(InstructionBlock* instruction_block) override;
private:
    Form form;
    bool link;
    u32 imm24;
    Register rm;
};