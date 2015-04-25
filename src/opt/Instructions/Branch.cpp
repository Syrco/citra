#include "Branch.h"
#include "CodeBlock.h"
#include "Codegen.h"

using namespace std;

bool Branch::CanCodegen(class Codegen *codegen, u32 pc)
{
	//return Cond() == Condition::AL;
	return codegen->CanCond(Cond());
}

bool Branch::IsDisabled(class Codegen *codegen, u32 pc)
{
	/*if (Form() == B && Cond() != Condition::AL)
	{
		static auto count = 0;
		++count;
		auto max = BinSearch(0x80).mid;
		if (count == max) cout << "Said no to " << hex << pc << endl;
		if (count >= max) return true;
	}*/
	return false;
}

void Branch::DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock)
{
	auto ib = codegen->irBuilder.get();
	auto delta = ((int)Imm() << 8) >> 6;
	auto target = codeBlock->pc + delta + 8;
	auto lr = ib->getInt32(codeBlock->pc + 4);
	if (Cond() == Condition::AL)
	{
		codeBlock->jumpAddress = target;
		if (Form() == BL)
			codeBlock->Write(Register::LR, lr);
	}
	else
	{
		auto passed = codegen->decoder->ConditionPassed(codeBlock, Cond());
		auto passed32 = ib->CreateSExt(passed, ib->getInt32Ty());
		auto notPassed32 = ib->CreateNot(passed32);

		auto pc = ib->CreateOr(ib->CreateAnd(passed32, ib->getInt32(target)), ib->CreateAnd(notPassed32, ib->getInt32(codeBlock->pc + 4)));
		if (Form() == BL)
			codeBlock->Write(Register::LR, ib->CreateOr(ib->CreateAnd(passed32, lr), ib->CreateAnd(notPassed32, codeBlock->Read(Register::LR))));
		codeBlock->WritePC(pc);
	}
}