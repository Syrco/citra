#include "Arithmetic.h"
#include "Disassembler.h"
#include <binary_translation/ARMFuncs.h>
#include <binary_translation/InstructionBlock.h>

static RegisterInstruction<Arithmetic> register_instruction;

bool IsSupported(Arithmetic::Op op)
{
    switch (op)
    {
    case Arithmetic::Op::BitwiseAnd: return true;
    case Arithmetic::Op::BitwiseXor: return true;
    case Arithmetic::Op::Subtract: return true;
    case Arithmetic::Op::RevSubtract: return true;
    case Arithmetic::Op::Add: return true;
    case Arithmetic::Op::AddWithCarry: return true;
    case Arithmetic::Op::SubtractWithCarry: return true;
    case Arithmetic::Op::ReverseSubtractWithCarry: return true;
    case Arithmetic::Op::BitwiseOr: return true;
    case Arithmetic::Op::MoveAndShifts: return false; // Handled in MovShift.cpp
    case Arithmetic::Op::BitwiseBitClear: return true;
    case Arithmetic::Op::BitwiseNot: return false; // MVN
    default: return false;
    }
}

bool IsBitwise(Arithmetic::Op op)
{
    switch (op)
    {
    case Arithmetic::Op::BitwiseAnd: return true;
    case Arithmetic::Op::BitwiseXor: return true;
    case Arithmetic::Op::BitwiseOr: return true;
    case Arithmetic::Op::BitwiseBitClear: return true;
    default: break;
    }
}

bool Arithmetic::Decode()
{
    if (ReadFields({ CondDef(), FieldDef<3>(0), FieldDef<4>(&op), FieldDef<1>(&s), FieldDef<4>(&rn),
                     FieldDef<4>(&rd), FieldDef<5>(&imm5), FieldDef<2>(&type), FieldDef<1>(0), FieldDef<4>(&rm)}))
    {
        form = Form::Register;
        if (rd == Register::PC && s) return false; // SEE SUBS PC, LR and related instructions;
        if (rn == Register::PC) return false;
        if (rm == Register::PC) return false;
        return IsSupported(op);
    }
    if (ReadFields({ CondDef(), FieldDef<3>(1), FieldDef<4>(&op), FieldDef<1>(&s), FieldDef<4>(&rn),
        FieldDef<4>(&rd), FieldDef<12>(&imm12) }))
    {
        form = Form::Immediate;
        if (rd == Register::PC && s) return false; // SEE SUBS PC, LR and related instructions;
        if (rn == Register::PC) return false;
        return IsSupported(op);
    }
    return false;
}

void Arithmetic::GenerateInstructionCode(InstructionBlock* instruction_block)
{
    auto ir_builder = instruction_block->Module()->IrBuilder();

    llvm::Value *left, *right, *carry_in;
    ARMFuncs::ResultCarryOverflow result = { nullptr, nullptr, nullptr };
    auto bitwise = IsBitwise(op);

    carry_in = instruction_block->Read(Register::C);

    left = instruction_block->Read(rn);

    if (form == Form::Register)
    {
        if (bitwise)
        {
            auto shift_tn = ARMFuncs::DecodeImmShift(instruction_block, type, imm5);
            auto shifted_carry = ARMFuncs::Shift_C(instruction_block, instruction_block->Read(rm), shift_tn.type, shift_tn.amount, carry_in);
            right = shifted_carry.result;
            result.carry = shifted_carry.carry;
        }
        else
        {
            auto shift_tn = ARMFuncs::DecodeImmShift(instruction_block, type, imm5);
            right = ARMFuncs::Shift(instruction_block, instruction_block->Read(rm), shift_tn.type, shift_tn.amount, carry_in);
        }
    }
    else
    {
        if (bitwise)
        {
            auto imm32_carry = ARMFuncs::ARMExpandImm_C(instruction_block, imm12, carry_in);
            right = imm32_carry.result;
            result.carry = imm32_carry.carry;
        }
        else
        {
            right = ARMFuncs::ARMExpandImm(instruction_block, imm12);
        }
    }

    switch (op)
    {
    case Op::BitwiseAnd: result.result = ir_builder->CreateAnd(left, right); break;
    case Op::BitwiseXor: result.result = ir_builder->CreateXor(left, right); break;
    case Op::Subtract: result = ARMFuncs::AddWithCarry(instruction_block, left, ir_builder->CreateNot(right), ir_builder->getInt32(1)); break;
    case Op::RevSubtract: result = ARMFuncs::AddWithCarry(instruction_block, ir_builder->CreateNot(left), right, ir_builder->getInt32(1)); break;
    case Op::Add: result = ARMFuncs::AddWithCarry(instruction_block, left, right, ir_builder->getInt32(0)); break;
    case Op::AddWithCarry: result = ARMFuncs::AddWithCarry(instruction_block, left, right, carry_in); break;
    case Op::SubtractWithCarry: result = ARMFuncs::AddWithCarry(instruction_block, left, ir_builder->CreateNot(right), carry_in); break;
    case Op::ReverseSubtractWithCarry: result = ARMFuncs::AddWithCarry(instruction_block, ir_builder->CreateNot(left), right, carry_in); break;
    case Op::BitwiseOr: result.result = ir_builder->CreateOr(left, right); break;
    case Op::BitwiseBitClear: result.result = ir_builder->CreateAnd(left, ir_builder->CreateNot(right)); break;
    default: break;
    }

    instruction_block->Write(rd, result.result);

    if (s)
    {
        instruction_block->Write(Register::N, ir_builder->CreateICmpSLT(result.result, ir_builder->getInt32(0)));
        instruction_block->Write(Register::Z, ir_builder->CreateICmpEQ(result.result, ir_builder->getInt32(0)));
        instruction_block->Write(Register::C, result.carry);
        if (result.overflow)
            instruction_block->Write(Register::V, result.overflow);
    }
}