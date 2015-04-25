#include "Mov.h"
#include "Codegen.h"
#include "Decode.h"
#include "CodeBlock.h"

using namespace llvm;

bool Mov::CanCodegen(Codegen *codegen, u32 pc)
{
	if (!codegen->CanCond(Cond())) return false;
	//if ((Form() == MovForms::Imm0 || Form() == MovForms::Reg) && S()) return false;
	if (Form() == MovForms::Reg && !codegen->CanRead(Rm())) return false;
	if (!codegen->CanWrite(Rd())) return false;

	return true;
}

bool Mov::IsDisabled(class Codegen* codegen, u32 pc)
{
	//return Form() == MovForms::Imm0 || Form() == MovForms::Imm1;
	//return Cond() != Condition::AL;
	return false;
}

void Mov::DoCodegen(Codegen *codegen, CodeBlock *codeBlock)
{
	auto ib = codegen->irBuilder.get();
	codeBlock->BeginCond(Cond());

	Decoder::ResultCarry result;
	result.carry = codeBlock->Read(Register::C);

	switch (Form())
	{
	case MovForms::Imm0:
		result = codegen->decoder->ARMExpandImm_C(Imm12(), result.carry);
		break;
	case MovForms::Imm1:
		result.result = ib->getInt32((Imm4() << 12) | Imm12());
		break;
	case MovForms::Reg:
		result.result = codeBlock->Read(Rm());
		switch (Op2())
		{
		case Operation2::LSL: if (Imm5() != 0) result = codegen->decoder->Shift_C32(result.result, SRType::LSL, codegen->decoder->DecodeImmShift(0, Imm5()).amount, result.carry); break;
		case Operation2::LSR: result = codegen->decoder->Shift_C32(result.result, SRType::LSR, codegen->decoder->DecodeImmShift(1, Imm5()).amount, result.carry); break;
		case Operation2::ASR: result = codegen->decoder->Shift_C32(result.result, SRType::ASR, codegen->decoder->DecodeImmShift(2, Imm5()).amount, result.carry); break;
		case Operation2::ROR:
			if (Imm5() != 0)
				result = codegen->decoder->Shift_C32(result.result, SRType::ROR, codegen->decoder->DecodeImmShift(3, Imm5()).amount, result.carry);
			else
				result = codegen->decoder->Shift_C32(result.result, SRType::RRX, ib->getInt32(1), result.carry);
			break;
		}
		break;
	}

	codeBlock->Write(Rd(), result.result);
	if ((Form() == MovForms::Imm0 || Form() == MovForms::Reg) && S())
	{
		codeBlock->Write(Register::N, ib->CreateICmpSLT(result.result, ib->getInt32(0)));
		codeBlock->Write(Register::Z, ib->CreateICmpEQ(result.result, ib->getInt32(0)));
		codeBlock->Write(Register::C, result.carry);
		// V unchanged
	}

	codeBlock->EndCond();
}