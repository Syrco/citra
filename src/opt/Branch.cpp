#include "Branch.h"
#include "CodeBlock.h"
#include "Codegen.h"

bool Branch::CanCodegen(class Codegen *codegen, u32 pc)
{
	return Cond() == Condition::AL;
}

void Branch::DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock)
{
	auto delta = ((int)Imm() << 8) >> 6;
	codeBlock->jumpAddress = codeBlock->pc + delta + 8;

	if (Form() == BL)
	{
		codeBlock->Write(Register::LR, codegen->irBuilder->getInt32(codeBlock->pc + 4));
	}
}