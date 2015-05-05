#include "Instruction.h"
#include "Types.h"

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