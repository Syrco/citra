#include "Instruction.h"
#include "Types.h"
#include <bitset>

class Str : public Instruction
{
public:
    enum class Form
    {
        Immediate, Reg, MultiReg
    };

public:
    virtual bool Decode() override;
    void GenerateInstructionCode(InstructionBlock* instruction_block) override;

private:
    Form form;
    bool U;
    Register rt;
    u32 imm12;
    bool P;
    bool W;
    Register rn;
    std::bitset<16> register_list;
};