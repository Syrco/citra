#include "MovShift.h"
#include "Disassembler.h"
#include "InstructionBlock.h"
#include "ModuleGen.h"
#include "ARMFuncs.h"

static RegisterInstruction<MovShift> register_instruction;

bool MovShift::Decode()
{
    // Mov and shifts must have zeroes at some operands of different data processing instructions
    if (ReadFields({ CondDef(), FieldDef<3>(0), FieldDef<4>(13), FieldDef<1>(&s), FieldDef<4>(0),
                     FieldDef<4>(&rd), FieldDef<5>(&imm5), FieldDef<2>(&op2), FieldDef<1>(0), FieldDef<4>(&rm) }))
    {
        form = Form::Register;
        if (rm == Register::PC) return false;
        if (rd == Register::PC && s) return false; // SEE SUBS PC, LR and related instructions;
        return true;
    }
    return false;
}

void MovShift::GenerateInstructionCode(InstructionBlock* instruction_block)
{
    auto ir_builder = instruction_block->IrBuilder();

    auto original_carry = instruction_block->Read(Register::C);
    ARMFuncs::ResultCarry result = { instruction_block->Read(rm), original_carry };

    switch (op2)
    {
    case Op2Type::MoveAndLSL:
        if (imm5 != 0)
        {
            result = ARMFuncs::Shift_C(instruction_block, result.result, ARMFuncs::SRType::LSL,
                ARMFuncs::DecodeImmShift(instruction_block, 0, imm5).amount, result.carry);
        }
        break;
    case Op2Type::LSR:
        result = ARMFuncs::Shift_C(instruction_block, result.result, ARMFuncs::SRType::LSR,
            ARMFuncs::DecodeImmShift(instruction_block, 1, imm5).amount, result.carry);
        break;
    case Op2Type::ASR:
        result = ARMFuncs::Shift_C(instruction_block, result.result, ARMFuncs::SRType::ASR,
            ARMFuncs::DecodeImmShift(instruction_block, 2, imm5).amount, result.carry);
        break;
    case Op2Type::RRXAndROR:
        if (imm5 == 0)
        {
            result = ARMFuncs::Shift_C(instruction_block, result.result, ARMFuncs::SRType::RRX,
                ir_builder->getInt32(1), result.carry);
        }
        else
        {
            result = ARMFuncs::Shift_C(instruction_block, result.result, ARMFuncs::SRType::ROR,
                ARMFuncs::DecodeImmShift(instruction_block, 3, imm5).amount, result.carry);
        }
        break;
    }

    instruction_block->Write(rd, result.result);

    if (s)
    {
        instruction_block->Write(Register::N, ir_builder->CreateTrunc(ir_builder->CreateLShr(result.result, 31), ir_builder->getInt1Ty()));
        instruction_block->Write(Register::Z, ir_builder->CreateICmpEQ(result.result, ir_builder->getInt32(0)));
        if (result.carry != original_carry)
            instruction_block->Write(Register::C, result.carry);
    }

    if (rd == Register::PC)
    {
        instruction_block->Module()->BranchReadPC();
    }
}