#include "Ldr.h"
#include "Codegen.h"
#include "CodeBlock.h"
#include <core/mem_map.h>

bool Ldr::CanCodegen(class Codegen *codegen, u32 pc)
{
	//if (pc == 0x100590) __debugbreak();
	if (Form() == LdrForms::LdrMultiReg && Cond() != Condition::AL && Regs()[15]) return false;
	if (Form() == LdrForms::StrMultiReg && Regs()[15]) return false;
	if (!codegen->CanCond(Cond())) return false;
	if ((Form() == LdrForms::Reg || Form() == LdrForms::PC) && IsRead() && !codegen->CanWrite(Rt())) return false;
	if (Form() == LdrForms::Reg && !IsRead() && !codegen->CanRead(Rt())) return false;
	if (Form() == LdrForms::Reg)
	{
		if (Rn() == Rt() && (P() == 0 || W() == 1)) return false; // UNPREDICTIBLE
		if (!codegen->CanRead(Rn())) return false;
		if (P() == 0 && W() == 1) return false; // LDRT, STRT
		if (Rn() == Register::SP && P() == 0 && U() == 1 && W() == 0 && Imm12() == 4) return false; // POP, PUSH
	}
	return true;

	/*auto base = pc + 8;
	auto address = U() ? base + Imm12() : base - Imm12();

	return address >= Codegen::TranslateStart && address <= Codegen::TranslateEnd - 4;*/
}

bool Ldr::IsDisabled(class Codegen* codegen, u32 pc)
{
	return false;
}

void Ldr::DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock)
{
	auto ib = codegen->irBuilder.get();

	if (Form() == LdrForms::LdrMultiReg || Form() == LdrForms::StrMultiReg)
	{
		auto regs = Regs();
		auto read = Form() == LdrForms::LdrMultiReg;
		auto doCond = !(read && regs[15]);
		if (doCond) codeBlock->BeginCond(Cond());

		auto wback = W() == 1;
		auto address = codeBlock->Read(Rn());
		llvm::Value *writeBackAddress = nullptr;
		if (!read) writeBackAddress = address = ib->CreateSub(address, ib->getInt32(4 * regs.count()));
		for (auto i = 0; i < 15; ++i)
		{
			if (!regs[i]) continue;
			if (read)
				codeBlock->Write((Register)i, codegen->Read32(address));
			else
				codegen->Write32(address, codeBlock->Read((Register)i));
			address = ib->CreateAdd(address, ib->getInt32(4));
		}
		if (regs[15])
		{
			codeBlock->WritePC(codegen->Read32(address));
			address = ib->CreateAdd(address, ib->getInt32(4));
		}
		if (wback) codeBlock->Write(Rn(), read ? address : writeBackAddress);

		if (doCond) codeBlock->EndCond();
	}
	else
	{
		codeBlock->BeginCond(Cond());
		llvm::Value *address = nullptr;
		llvm::Value *value = nullptr;

		auto add = U() == 1;
		auto read = true;

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
			read = IsRead();
			auto index = P() == 1;
			auto wback = P() == 0 || W() == 1;
			auto rn = codeBlock->Read(Rn());
			auto imm32 = ib->getInt32(Imm12());

			auto offsetAddr = add ? ib->CreateAdd(rn, imm32) : ib->CreateSub(rn, imm32);
			address = index ? offsetAddr : rn;
			if (wback)
				codeBlock->Write(Rn(), offsetAddr);
		}
		if (read)
		{
			if (!value) value = codegen->Read32(address);
			codeBlock->Write(Rt(), value);
		}
		else
		{
			codegen->Write32(address, codeBlock->Read(Rt()));
		}
		codeBlock->EndCond();
	}
}