#include "Instructions.h"
#include "Codegen.h"
#include "CodeBlock.h"

template InstructionsBase<INSTRUCTIONS>;

InstructionBase::~InstructionBase()
{
}

bool InstructionBase::IsDisabled(class Codegen* codegen, u32 pc)
{
	return false;
}