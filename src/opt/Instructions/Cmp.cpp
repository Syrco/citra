#include "Cmp.h"
#include "Codegen.h"
#include "Decode.h"
#include "CodeBlock.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/InlineAsm.h"

using namespace llvm;

bool Cmp::CanCodegen(Codegen *codegen, u32 pc)
{
	return codegen->CanCond(Cond());
	//return Cond() == Condition::AL;
}

bool Cmp::IsDisabled(class Codegen* codegen, u32 pc)
{
	return false;
	//return Form() == CmpForms::Reg;
	//return true;
	//return Form() == CmpForms::Reg || Form() == CmpForms::RegSReg;
}

void Cmp::DoCodegen(Codegen *codegen, CodeBlock *codeBlock)
{
	//if (codeBlock->pc == 0x115168) __debugbreak();
	codeBlock->BeginCond(Cond());
	auto rn = codeBlock->Read(Rn());

	llvm::Value *compareArg2;
	if (Form() == CmpForms::Imm)
	{
		compareArg2 = codegen->decoder->ARMExpandImm(Imm12());
	}
	else
	{
		Decoder::ShiftTN shift;
		if (Form() == CmpForms::Reg)
		{
			shift = codegen->decoder->DecodeImmShift(Type(), Imm5());
		}
		else
		{
			shift.type = codegen->decoder->DecodeRegShift(Type());
			shift.amount = codeBlock->Read(Rs());
		}
		compareArg2 = codegen->decoder->Shift32(codeBlock->Read(Rm()), shift.type, shift.amount, codeBlock->Read(Register::C));
	}

	auto rco = codegen->decoder->AddWithCarry3232(rn, codegen->irBuilder->CreateNot(compareArg2), codegen->irBuilder->getInt32(1));

	auto n = codegen->irBuilder->CreateICmpSLT(rco.result, codegen->irBuilder->getInt32(0));
	auto z = codegen->irBuilder->CreateICmpEQ(rco.result, codegen->irBuilder->getInt32(0));
	auto c = rco.carry;
	auto v = rco.overflow;
	
	codeBlock->Write(Register::N, n);
	codeBlock->Write(Register::Z, z);
	codeBlock->Write(Register::C, c);
	codeBlock->Write(Register::V, v);
	codeBlock->EndCond();
}