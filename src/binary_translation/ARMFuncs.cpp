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

    // amount_zero_basic_block will not recieve any code, it used only for the phi
    auto amount_zero_basic_block = instruction->CreateBasicBlock("ShiftCAmount0");
    auto amount_not_zero_basic_block = instruction->CreateBasicBlock("ShiftCAmountNot0");
    auto phi_basic_block = instruction->CreateBasicBlock("ShiftCPhi");

    ir_builder->CreateCondBr(ir_builder->CreateICmpEQ(amount, ir_builder->getInt32(0)), amount_zero_basic_block, amount_not_zero_basic_block);

    ir_builder->SetInsertPoint(amount_zero_basic_block);
    ir_builder->CreateBr(phi_basic_block);

    ir_builder->SetInsertPoint(amount_not_zero_basic_block);
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
    auto pred = ir_builder->GetInsertBlock(); // The basic block might have changed and needs to be current for the phi
    ir_builder->CreateBr(phi_basic_block);

    ir_builder->SetInsertPoint(phi_basic_block);
    auto result_phi = ir_builder->CreatePHI(ir_builder->getInt32Ty(), 2);
    auto carry_phi = ir_builder->CreatePHI(ir_builder->getInt1Ty(), 2);

    result_phi->addIncoming(value, amount_zero_basic_block);
    result_phi->addIncoming(result_amount_not_zero.result, pred);
    carry_phi->addIncoming(carry_in, amount_zero_basic_block);
    carry_phi->addIncoming(result_amount_not_zero.carry, pred);

    return{ result_phi, carry_phi };
}

// Generates code for LSL, LSR that checks for 0 shift
llvm::Value* ShiftZeroCheck(
    InstructionBlock *instruction, llvm::Value* x, llvm::Value* shift,
    std::function<ARMFuncs::ResultCarry(InstructionBlock *, llvm::Value*, llvm::Value*)> non_zero_function)
{
    auto ir_builder = instruction->IrBuilder();

    // amount_zero_basic_block will not recieve any code, it used only for the phi
    auto amount_zero_basic_block = instruction->CreateBasicBlock("ShiftZeroCheckAmount0");
    auto amount_not_zero_basic_block = instruction->CreateBasicBlock("ShiftZeroCheckAmountNot0");
    auto phi_basic_block = instruction->CreateBasicBlock("ShiftZeroCheckPhi");

    ir_builder->CreateCondBr(ir_builder->CreateICmpEQ(shift, ir_builder->getInt32(0)), amount_zero_basic_block, amount_not_zero_basic_block);

    ir_builder->SetInsertPoint(amount_zero_basic_block);
    ir_builder->CreateBr(phi_basic_block);

    ir_builder->SetInsertPoint(amount_not_zero_basic_block);
    auto result_amount_not_zero = non_zero_function(instruction, x, shift);
    auto pred = ir_builder->GetInsertBlock(); // The basic block might have changed and needs to be current for the phi
    ir_builder->CreateBr(phi_basic_block);

    ir_builder->SetInsertPoint(phi_basic_block);
    auto phi = ir_builder->CreatePHI(ir_builder->getInt32Ty(), 2);

    phi->addIncoming(x, amount_zero_basic_block);
    phi->addIncoming(result_amount_not_zero.result, pred);

    return phi;
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