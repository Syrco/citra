#include "Arith.h"
#include "Codegen.h"
#include "Decode.h"
#include "CodeBlock.h"
#include <iostream>

using namespace llvm;

bool Arith::CanCodegen(Codegen *codegen, u32 pc)
{
	if (!codegen->CanCond(Cond())) return false;
	if (!codegen->CanWrite(Rd())) return false;
	switch (Operation())
	{
	case Op::BitAnd: case Op::BitXor: case Op::BitOr: case Op::BitClear:
	case Op::Sub: case Op::RSub: case Op::Add: case Op::AddC: case Op::SubC: case Op::RSubC:
		break;
	default:
		return false;
	}

	return true;
}

bool Arith::IsDisabled(Codegen *codegen, u32 pc)
{
	//if (S()) return true;
	return false;
	if (Operation() == Op::BitAnd) return false;
	if (Operation() == Op::BitXor) return false;
	if (Operation() == Op::BitOr) return false;
	if (Operation() == Op::Add) return false;
	/*if (Operation() == Op::Add)
	{
		static size_t count = 0;
		++count;
		auto max = BinSearch(0x1000).l().r().l().l().l().l().r().r().l().l().l().mid;
		if (count < max) return false;
		if (count == max) std::cout << "Said no to " << std::hex << pc << std::endl;
	}*/
	return true;
}

void Arith::DoCodegen(Codegen *codegen, CodeBlock *codeBlock)
{
	auto ib = codegen->irBuilder.get();
	auto dec = codegen->decoder;
	codeBlock->BeginCond(Cond());

	llvm::Value *rn;
	if (Rn() == Register::PC)
		rn = ib->getInt32(codeBlock->pc + 8);
	else
		rn = codeBlock->Read(Rn());
	auto c = codeBlock->Read(Register::C);
	Decoder::ResultCarry imm{};
	switch (Operation())
	{
	case Op::BitAnd: case Op::BitXor: case Op::BitOr: case Op::BitClear:
		imm = codegen->decoder->ARMExpandImm_C(Imm12(), c); break;
	case Op::Sub: case Op::RSub: case Op::Add: case Op::AddC: case Op::SubC: case Op::RSubC:
		imm.result = codegen->decoder->ARMExpandImm(Imm12()); break;
	default: __debugbreak();
	}
	Decoder::ResultCarryOverflow rsc = { imm.result, imm.carry, codeBlock->Read(Register::V) };

	switch (Operation())
	{
	case Op::BitAnd: rsc.result = ib->CreateAnd(rn, imm.result); break;
	case Op::BitXor: rsc.result = ib->CreateXor(rn, imm.result); break;
	case Op::Sub: rsc = dec->AddWithCarry3232(rn, ib->CreateNot(imm.result), ib->getInt32(1)); break;
	case Op::RSub: rsc = dec->AddWithCarry3232(ib->CreateNot(rn), imm.result, ib->getInt32(1)); break;
	case Op::Add: rsc = dec->AddWithCarry3232(rn, imm.result, ib->getInt32(0)); break;
	case Op::AddC: rsc = dec->AddWithCarry3232(rn, imm.result, c); break;
	case Op::SubC: rsc = dec->AddWithCarry3232(rn, ib->CreateNot(imm.result), c); break;
	case Op::RSubC: rsc = dec->AddWithCarry3232(ib->CreateNot(rn), imm.result, c); break;
	case Op::BitOr: rsc.result = ib->CreateOr(rn, imm.result); break;
	case Op::BitClear: rsc.result = ib->CreateAnd(rn, ib->CreateNot(imm.result)); break;
	default: __debugbreak();
	}

	codeBlock->Write(Rd(), rsc.result);
	if (S())
	{
		codeBlock->Write(Register::N, ib->CreateICmpSLT(rsc.result, ib->getInt32(0)));
		codeBlock->Write(Register::Z, ib->CreateICmpEQ(rsc.result, ib->getInt32(0)));
		codeBlock->Write(Register::C, rsc.carry);
		codeBlock->Write(Register::V, rsc.overflow);
	}

	codeBlock->EndCond();
}