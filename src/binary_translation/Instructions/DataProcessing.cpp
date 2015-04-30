#include "DataProcessing.h"
#include "Disassembler.h"
#include "InstructionBlock.h"
#include "ModuleGen.h"

static RegisterInstruction<DataProcessing> register_instruction;

bool DataProcessing::Decode()
{
    // Mov and shifts must have zeroes at some operands of different data processing instructions
    if (ReadFields({ FieldDef<4>(&cond), FieldDef<3>(0), FieldDef<4>((u32)ShortOpType::MoveAndShifts), FieldDef<1>(&s), FieldDef<4>(0),
                     FieldDef<4>(&rd), FieldDef<5>(&imm5), FieldDef<3>(0), FieldDef<4>(&rm) }))
    {
        form = Form::Register;
        if (cond != Condition::AL) return false;
        if (imm5 != 0) return false; // Shifts
        if (s != 0) return false; // Set flags
        if (rm == Register::PC) return false; // Jump
        return true;
    }
    if (ReadFields({ FieldDef<4>(&cond), FieldDef<3>(1), FieldDef<4>(&short_op), FieldDef<1>(&s), FieldDef<4>(&rn),
        FieldDef<4>(&rd), FieldDef<12>(&imm12) }))
    {
        // TODO: not implemented
        form = Form::Immediate;
        return false;
    }
    return false;
}

void DataProcessing::GenerateCode(InstructionBlock* instruction_block)
{
    // Currently supports only mov reg, reg

    auto value = instruction_block->Read(rm);
    instruction_block->Write(rd, value);

    if (rd == Register::PC)
    {
        instruction_block->Module()->BranchReadPC();
    }
}