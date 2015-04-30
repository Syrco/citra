#include "Branch.h"
#include "Disassembler.h"
#include "InstructionBlock.h"
#include "ModuleGen.h"

static RegisterInstruction<Branch> register_instruction;

bool Branch::Decode()
{
    // B imm, BL imm
    if (ReadFields({ CondDef(), FieldDef<3>(5), FieldDef<1>(&link), FieldDef<24>(&imm24) }))
    {
        form = Form::Immediate;
        return true;
    }
    // BLX reg
    if (ReadFields({ CondDef(), FieldDef<24>(0x12fff3), FieldDef<4>(&rm) }))
    {
        if (rm == Register::PC) return false;

        link = true;
        form = Form::Register;
        return true;
    }
    return false;
}

void Branch::GenerateInstructionCode(InstructionBlock* instruction_block)
{
    auto ir_builder = instruction_block->Module()->IrBuilder();
    if (link)
    {
        instruction_block->Write(Register::LR, ir_builder->getInt32(instruction_block->Address() + 4));
    }
    if (form == Form::Immediate)
    {
        auto pc = static_cast<s32>(imm24 << 2);
        pc = pc << 6 >> 6; // Sign extend
        pc += instruction_block->Address() + 8;
        instruction_block->Module()->BranchWritePCConst(pc);
    }
    else
    {
        auto pc = instruction_block->Read(rm);
        instruction_block->Write(Register::PC, pc);
        instruction_block->Module()->BranchReadPC();
    }
}