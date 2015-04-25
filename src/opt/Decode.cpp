#include "Decode.h"
#include "Codegen.h"
#include "llvm/IR/Intrinsics.h"
#include "CodeBlock.h"

using namespace llvm;
using ResultCarry = Decoder::ResultCarry;
using ResultCarryOverflow = Decoder::ResultCarryOverflow;
using ShiftTN = Decoder::ShiftTN;

Decoder::Decoder(Codegen *codegen) : codegen(codegen)
{

}

ShiftTN Decoder::DecodeImmShift(u32 type, u32 imm5)
{
	switch (type)
	{
	case 0: return{ SRType::LSL, codegen->irBuilder->getInt32(imm5) };
	case 1: return{ SRType::LSR, codegen->irBuilder->getInt32(imm5 ? imm5 : 32) };
	case 2: return{ SRType::ASR, codegen->irBuilder->getInt32(imm5 ? imm5 : 32) };
	case 3:
		if (imm5)
			return{ SRType::ROR, codegen->irBuilder->getInt32(imm5) };
		else
			return{ SRType::RRX, codegen->irBuilder->getInt32(1) };
	}
}

SRType Decoder::DecodeRegShift(u32 type)
{
	switch (type)
	{
	case 0: return SRType::LSL;
	case 1: return SRType::LSR;
	case 2: return SRType::ASR;
	case 3: return SRType::ROR;
	}
}

llvm::Value *Decoder::Shift32(llvm::Value *value, SRType type, llvm::Value *amount, llvm::Value *carry)
{
	return Shift_C32(value, type, amount, carry).result;
}
ResultCarry Decoder::Shift_C32(Value *value, SRType type, llvm::Value *amount, Value *carry)
{
	ResultCarry resultNot0;
	switch (type)
	{
	case SRType::LSL: resultNot0 = LSL_C32(value, amount); break;
	case SRType::LSR: resultNot0 = LSR_C32(value, amount); break;
	case SRType::ASR: resultNot0 = ASR_C32(value, amount); break;
	case SRType::ROR: resultNot0 = ROR_C32(value, amount); break;
	case SRType::RRX: resultNot0 = RRX_C32(value, carry); break;
	}
	ResultCarry result0 = { value, carry };

	auto ib = codegen->irBuilder.get();

	auto ifAmount0I1 = ib->CreateICmpEQ(amount, ib->getInt32(0));
	auto ifAmountNot0I1 = ib->CreateNot(ifAmount0I1);

	auto ifAmount0I32 = ib->CreateSExt(ifAmount0I1, ib->getInt32Ty());
	auto ifAmountNot0I32 = ib->CreateSExt(ifAmountNot0I1, ib->getInt32Ty());

	auto result = ib->CreateOr(ib->CreateAnd(result0.result, ifAmount0I32), ib->CreateAnd(resultNot0.result, ifAmountNot0I32));
	carry = ib->CreateOr(ib->CreateAnd(result0.carry, ifAmount0I1), ib->CreateAnd(resultNot0.carry, ifAmountNot0I1));
	return{ result, carry };
}

llvm::Value *Decoder::LSL32(llvm::Value *value, llvm::Value *amount)
{
	auto ib = codegen->irBuilder.get();
	//ib->CreateAssumption(ib->CreateICmpSGE(amount, ib->getInt32(0)));
	auto ifAmount0I1 = ib->CreateICmpEQ(amount, ib->getInt32(0));
	auto ifAmountNot0I1 = ib->CreateNot(ifAmount0I1);
	auto ifAmount0I32 = ib->CreateSExt(ifAmount0I1, ib->getInt32Ty());
	auto ifAmountNot0I32 = ib->CreateSExt(ifAmountNot0I1, ib->getInt32Ty());

	return ib->CreateOr(ib->CreateAnd(value, ifAmount0I32), ib->CreateAnd(LSL_C32(value, amount).result, ifAmountNot0I32));
}
ResultCarry Decoder::LSL_C32(llvm::Value *value, llvm::Value *amount)
{
	auto ib = codegen->irBuilder.get();
	//ib->CreateAssumption(ib->CreateICmpSGT(amount, ib->getInt32(0)));
	auto result = ib->CreateShl(value, amount);
	auto carry = ib->CreateTrunc(ib->CreateLShr(value, ib->CreateSub(ib->getInt32(32), amount, "", true, true)), ib->getInt1Ty());
	return{ result, carry };
}

llvm::Value *Decoder::LSR32(llvm::Value *value, llvm::Value *amount)
{
	auto ib = codegen->irBuilder.get();
	//ib->CreateAssumption(ib->CreateICmpSGE(amount, ib->getInt32(0)));
	auto ifAmount0I1 = ib->CreateICmpEQ(amount, ib->getInt32(0));
	auto ifAmountNot0I1 = ib->CreateNot(ifAmount0I1);
	auto ifAmount0I32 = ib->CreateSExt(ifAmount0I1, ib->getInt32Ty());
	auto ifAmountNot0I32 = ib->CreateSExt(ifAmountNot0I1, ib->getInt32Ty());

	return ib->CreateOr(ib->CreateAnd(value, ifAmount0I32), ib->CreateAnd(LSR_C32(value, amount).result, ifAmountNot0I32));
}
ResultCarry Decoder::LSR_C32(llvm::Value *value, llvm::Value *amount)
{
	auto ib = codegen->irBuilder.get();
	//ib->CreateAssumption(ib->CreateICmpSGT(amount, ib->getInt32(0)));
	auto result = ib->CreateLShr(value, amount);
	auto carry = ib->CreateTrunc(ib->CreateLShr(value, ib->CreateSub(amount, ib->getInt32(1), "", true, true)), ib->getInt1Ty());
	return{ result, carry };
}

ResultCarry Decoder::ASR_C32(llvm::Value *value, llvm::Value *amount)
{
	auto ib = codegen->irBuilder.get();
	//ib->CreateAssumption(ib->CreateICmpSGT(amount, ib->getInt32(0)));
	auto result = ib->CreateAShr(value, amount);
	auto carry = ib->CreateTrunc(ib->CreateLShr(value, ib->CreateSub(amount, ib->getInt32(1), "", true, true)), ib->getInt1Ty());
	return{ result, carry };
}

ResultCarry Decoder::ROR_C32(llvm::Value *value, llvm::Value *amount)
{
	auto ib = codegen->irBuilder.get();
	//ib->CreateAssumption(ib->CreateICmpNE(amount, ib->getInt32(0)));
	auto N = ib->getInt32(32);
	auto m = ib->CreateSRem(amount, N);
	auto result = ib->CreateOr(LSR32(value, m), LSL32(value, ib->CreateSub(N, m)));
	auto carry = codegen->irBuilder->CreateTrunc(codegen->irBuilder->CreateLShr(result, 32 - 1), codegen->irBuilder->getInt1Ty());
	return{ result, carry };
}

ResultCarry Decoder::RRX_C32(llvm::Value *value, llvm::Value *carry)
{
	auto ib = codegen->irBuilder.get();
	auto result = ib->CreateLShr(value, 1);
	result = ib->CreateOr(result, ib->CreateShl(ib->CreateZExt(carry, ib->getInt32Ty()), 31));
	carry = ib->CreateTrunc(value, ib->getInt1Ty());
	return{ result, carry };
}

llvm::Value *Decoder::ARMExpandImm(u32 imm12)
{
	return ARMExpandImm_C(imm12, codegen->irBuilder->getInt1(0)).result;
}
ResultCarry Decoder::ARMExpandImm_C(u32 imm12, llvm::Value *carry)
{
	auto ib = codegen->irBuilder.get();
	auto unrotated = imm12 & 0xFF;
	return Shift_C32(codegen->irBuilder->getInt32(unrotated), SRType::ROR, ib->getInt32(2 * (imm12 >> 8)), carry);
}
ResultCarryOverflow Decoder::AddWithCarry3232(llvm::Value *x, llvm::Value *y, llvm::Value *carryIn)
{
	auto ib = codegen->irBuilder.get();
	auto xu64 = ib->CreateZExt(x, ib->getInt64Ty());
	auto xs64 = ib->CreateSExt(x, ib->getInt64Ty());
	auto yu64 = ib->CreateZExt(y, ib->getInt64Ty());
	auto ys64 = ib->CreateSExt(y, ib->getInt64Ty());
	auto c64 = ib->CreateZExt(carryIn, ib->getInt64Ty());

	auto unsignedSum = ib->CreateAdd(ib->CreateAdd(xu64, yu64), c64);
	auto singedSum = ib->CreateAdd(ib->CreateAdd(xs64, ys64), c64);
	auto result32 = ib->CreateTrunc(unsignedSum, ib->getInt32Ty());
	auto resultU64 = ib->CreateZExt(result32, ib->getInt64Ty());
	auto resultS64 = ib->CreateSExt(result32, ib->getInt64Ty());

	auto carry = ib->CreateICmpNE(resultU64, unsignedSum);
	auto overflow = ib->CreateICmpNE(resultS64, singedSum);

	return{ result32, carry, overflow };

	/*auto ib = codegen->irBuilder.get();

	auto uadd = Intrinsic::getDeclaration(codegen->module.get(), Intrinsic::uadd_with_overflow, { ib->getInt32Ty() });
	auto sadd = Intrinsic::getDeclaration(codegen->module.get(), Intrinsic::sadd_with_overflow, { ib->getInt32Ty() });

	auto uadd1 = ib->CreateCall2(uadd, x, y);
	auto uadd2 = ib->CreateCall2(uadd, ib->CreateExtractValue(uadd1, 0), carryIn);
	auto sadd1 = ib->CreateCall2(sadd, x, y);
	auto sadd2 = ib->CreateCall2(sadd, ib->CreateExtractValue(sadd1, 0), carryIn);

	auto carry = ib->CreateOr(ib->CreateExtractValue(uadd1, 1), ib->CreateExtractValue(uadd2, 1));
	auto overflow = ib->CreateXor(ib->CreateExtractValue(sadd1, 1), ib->CreateExtractValue(sadd2, 1));

	return{ ib->CreateExtractValue(uadd2, 0), carry, overflow };*/
}

llvm::Value *Decoder::ConditionPassed(class CodeBlock *codeBlock, Condition cond)
{
	llvm::Value *pred;
	bool not = false;
	switch (cond)
	{
	case Condition::NE: case Condition::CC: case Condition::PL: case Condition::VC:
	case Condition::LS: case Condition::LT: case Condition::LE:
		not = true;
		cond = (Condition)((int)cond - 1);
	}

	switch (cond)
	{
	case Condition::EQ: pred = codeBlock->Read(Register::Z); break;
	case Condition::CS: pred = codeBlock->Read(Register::C); break;
	case Condition::MI: pred = codeBlock->Read(Register::N); break;
	case Condition::VS: pred = codeBlock->Read(Register::V); break;
	case Condition::HI: pred = codegen->irBuilder->CreateAnd(codeBlock->Read(Register::C), codegen->irBuilder->CreateNot(codeBlock->Read(Register::Z))); break;
	case Condition::GE: pred = codegen->irBuilder->CreateICmpEQ(codeBlock->Read(Register::N), codeBlock->Read(Register::V)); break;
	case Condition::GT: pred = codegen->irBuilder->CreateAnd(codegen->irBuilder->CreateNot(codeBlock->Read(Register::Z)),
		codegen->irBuilder->CreateICmpEQ(codeBlock->Read(Register::N), codeBlock->Read(Register::V))); break;
	case Condition::AL: pred = codegen->irBuilder->getInt1(true);
	}

	if (not) pred = codegen->irBuilder->CreateNot(pred);
	return pred;
}

void Decoder::CreateConditionPassed(class CodeBlock *codeBlock, Condition cond, llvm::BasicBlock *passed, llvm::BasicBlock *notPassed)
{
	codegen->irBuilder->CreateCondBr(ConditionPassed(codeBlock, cond), passed, notPassed);
}