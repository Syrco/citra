#include "Ldr.h"
#include "Codegen.h"
#include "CodeBlock.h"
#include <core/mem_map.h>

bool Ldr::CanCodegen(class Codegen *codegen, u32 pc)
{
	if (Form() == LdrForms::MultiReg && Regs()[15]) return false;
	if (Form() == LdrForms::MultiReg && Cond() != Condition::AL && Regs()[15]) return false;
	if (!codegen->CanCond(Cond())) return false;
	if ((Form() == LdrForms::Reg || Form() == LdrForms::PC) && !codegen->CanWrite(Rt())) return false;
	if (Form() == LdrForms::Reg)
	{
		if (!codegen->CanRead(Rn())) return false;
		if (P() == 0 && W() == 1) return false; // LDRT
		if (Rn() == Register::SP && P() == 0 && U() == 1 && W() == 0 && Imm12() == 4) return false; // POP
	}
	return true;

	/*auto base = pc + 8;
	auto address = U() ? base + Imm12() : base - Imm12();

	return address >= Codegen::TranslateStart && address <= Codegen::TranslateEnd - 4;*/
}

bool Ldr::IsDisabled(class Codegen *codegen)
{
	//return Form() == LdrForms::Reg;
	return false;
}

void Ldr::DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock)
{
	auto ib = codegen->irBuilder.get();

	if (Form() == LdrForms::MultiReg)
	{
		auto regs = Regs();
		if (!regs[15]) codeBlock->BeginCond(Cond());

		auto wback = W() == 1;
		auto address = codeBlock->Read(Rn());
		for (auto i = 0; i < 16; ++i)
		{
			if (!regs[i]) continue;
			codeBlock->Write((Register)i, codegen->Read32(address));
			address = ib->CreateAdd(address, ib->getInt32(4));
		}
		if (wback) codeBlock->Write(Rn(), address);

		if (!regs[15]) codeBlock->EndCond();
	}
	else
	{
		codeBlock->BeginCond(Cond());
		llvm::Value *address = nullptr;
		llvm::Value *value = nullptr;

		auto add = U() == 1;

		if (Form() == LdrForms::PC)
		{
			auto base = codeBlock->pc + 8;
			auto constAddress = add ? base + Imm12() : base - Imm12();

			if (constAddress >= Codegen::TranslateStart && constAddress <= Codegen::TranslateEnd - 4)
				value = ib->getInt32(Memory::Read32(constAddress));
			else
				address = ib->getInt32(constAddress);
		}
		else if (Form() == LdrForms::Reg)
		{
			auto index = P() == 1;
			auto wback = P() == 0 || W() == 1;
			auto rn = codeBlock->Read(Rn());
			auto imm32 = ib->getInt32(Imm12());

			auto offsetAddr = add ? ib->CreateAdd(rn, imm32) : ib->CreateSub(rn, imm32);
			address = index ? offsetAddr : rn;
			if (wback)
				//codeBlock->Write(Rn(), ib->CreateAdd(rn, ib->getInt32(4)));
				codeBlock->Write(Rn(), offsetAddr);
		}
		if (!value && address)
		{
			value = codegen->Read32(address);
		}
		codeBlock->Write(Rt(), value);
		codeBlock->EndCond();
	}
}