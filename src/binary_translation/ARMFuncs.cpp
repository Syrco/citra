#include "ARMFuncs.h"
#include "InstructionBlock.h"

ARMFuncs::ShiftTN ARMFuncs::DecodeImmShift(InstructionBlock* instruction, u32 type, u32 imm5)
{
    auto ir_builder = instruction->IrBuilder();
    switch (type)
    {
    case 0: return{ SRType::LSL, ir_builder->getInt32(imm5) };
    case 1: return{ SRType::LSR, ir_builder->getInt32(imm5 ? imm5 : 32) };
    case 2: return{ SRType::ASR, ir_builder->getInt32(imm5 ? imm5 : 32) };
    case 3:
        if (imm5)
            return{ SRType::ROR, ir_builder->getInt32(imm5) };
        else
            return{ SRType::RRX, ir_builder->getInt32(1) };
    default: assert(false, "Invalid shift type");
    }
}

ARMFuncs::SRType ARMFuncs::DecodeRegShift(u32 type)
{
    switch (type)
    {
    case 0: return SRType::LSL;
    case 1: return SRType::LSR;
    case 2: return SRType::ASR;
    case 3: return SRType::ROR;
    default: assert(false, "Invalid shift type");
    }
}

llvm::Value* ARMFuncs::Shift(InstructionBlock* instruction, llvm::Value* value, SRType type, llvm::Value* amount, llvm::Value* carry_in)
{
    return Shift_C(instruction, value, type, amount, carry_in).result;
}

ARMFuncs::ResultCarry ARMFuncs::Shift_C(InstructionBlock* instruction, llvm::Value* value, SRType type, llvm::Value* amount, llvm::Value* carry_in)
{
    auto ir_builder = instruction->IrBuilder();

	auto amount_zero = ir_builder->CreateICmpEQ(amount, ir_builder->getInt32(0));
    ResultCarry result_amount_not_zero = {};
    switch (type)
    {
    case SRType::LSL: result_amount_not_zero = LSL_C(instruction, value, amount); break;
    case SRType::LSR: result_amount_not_zero = LSR_C(instruction, value, amount); break;
    case SRType::ASR: result_amount_not_zero = ASR_C(instruction, value, amount); break;
    case SRType::ROR: result_amount_not_zero = ROR_C(instruction, value, amount); break;
    case SRType::RRX: result_amount_not_zero = RRX_C(instruction, value, carry_in); break;
    default: assert(false, "Invalid shift type");
    }

	auto result = ir_builder->CreateSelect(amount_zero, value, result_amount_not_zero.result);
	auto carry = ir_builder->CreateSelect(amount_zero, carry_in, result_amount_not_zero.carry);

    return{ result, carry };
}

// Generates code for LSL, LSR that checks for 0 shift
llvm::Value* ShiftZeroCheck(
    InstructionBlock *instruction, llvm::Value* x, llvm::Value* shift,
    std::function<ARMFuncs::ResultCarry(InstructionBlock *, llvm::Value*, llvm::Value*)> non_zero_function)
{
    auto ir_builder = instruction->IrBuilder();

	auto amount_zero = ir_builder->CreateICmpEQ(shift, ir_builder->getInt32(0));
    auto result_amount_not_zero = non_zero_function(instruction, x, shift);

	return ir_builder->CreateSelect(amount_zero, x, result_amount_not_zero.result);
}

ARMFuncs::ResultCarry ARMFuncs::LSL_C(InstructionBlock* instruction, llvm::Value* x, llvm::Value* shift)
{
    auto ir_builder = instruction->IrBuilder();
    auto N = ir_builder->getInt32(32);

    auto result = ir_builder->CreateShl(x, shift);
    auto carry = ir_builder->CreateTrunc(ir_builder->CreateLShr(x, ir_builder->CreateSub(N, shift, "", true, true)), ir_builder->getInt1Ty());
    return{ result, carry };
}

llvm::Value* ARMFuncs::LSL(InstructionBlock* instruction, llvm::Value* x, llvm::Value* shift)
{
    return ShiftZeroCheck(instruction, x, shift, &ARMFuncs::LSL_C);
}

ARMFuncs::ResultCarry ARMFuncs::LSR_C(InstructionBlock* instruction, llvm::Value* x, llvm::Value* shift)
{
    auto ir_builder = instruction->IrBuilder();
    auto one = ir_builder->getInt32(1);

    auto result = ir_builder->CreateLShr(x, shift);
    auto carry = ir_builder->CreateTrunc(ir_builder->CreateLShr(x, ir_builder->CreateSub(shift, one, "", true, true)), ir_builder->getInt1Ty());
    return{ result, carry };
}

llvm::Value* ARMFuncs::LSR(InstructionBlock* instruction, llvm::Value* x, llvm::Value* shift)
{
    return ShiftZeroCheck(instruction, x, shift, &ARMFuncs::LSR_C);
}

ARMFuncs::ResultCarry ARMFuncs::ASR_C(InstructionBlock* instruction, llvm::Value* x, llvm::Value* shift)
{
    auto ir_builder = instruction->IrBuilder();
    auto one = ir_builder->getInt32(1);

    auto result = ir_builder->CreateAShr(x, shift);
    auto carry = ir_builder->CreateTrunc(ir_builder->CreateLShr(x, ir_builder->CreateSub(shift, one, "", true, true)), ir_builder->getInt1Ty());
    return{ result, carry };
}

ARMFuncs::ResultCarry ARMFuncs::ROR_C(InstructionBlock* instruction, llvm::Value* x, llvm::Value* shift)
{
    auto ir_builder = instruction->IrBuilder();
    auto N = ir_builder->getInt32(32);
    auto m = ir_builder->CreateURem(shift, N);

    auto result = ir_builder->CreateOr(LSR(instruction, x, m), LSL(instruction, x, ir_builder->CreateSub(N, m)));
    auto carry = ir_builder->CreateTrunc(ir_builder->CreateLShr(result, ir_builder->getInt32(31)), ir_builder->getInt1Ty());
    return{ result, carry };
}

ARMFuncs::ResultCarry ARMFuncs::RRX_C(InstructionBlock* instruction, llvm::Value* x, llvm::Value* carry_in)
{
    auto ir_builder = instruction->IrBuilder();

    auto result = ir_builder->CreateLShr(x, 1);
    result = ir_builder->CreateOr(result, ir_builder->CreateShl(ir_builder->CreateZExt(carry_in, ir_builder->getInt32Ty()), 31));
    auto carry = ir_builder->CreateTrunc(x, ir_builder->getInt1Ty());
    return{ result, carry };
}

ARMFuncs::ResultCarry ARMFuncs::ARMExpandImm_C(InstructionBlock *instruction, u32 imm12, llvm::Value* carry)
{
	auto ir_builder = instruction->IrBuilder();

	auto value = ir_builder->getInt32(imm12 & 0xFF);
	auto shift = ir_builder->getInt32(2 * (imm12 >> 8));
	return Shift_C(instruction, value, SRType::ROR, shift, carry);
}