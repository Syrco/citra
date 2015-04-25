#pragma once
#include "Instruction.h"

enum BranchForm
{
	B, BL
};

class Branch : public Instruction<
	Form<B , Field<4>, Bits<1,0,1,0>, Field<24>>,
	Form<BL, Field<4>, Bits<1,0,1,1>, Field<24>>
>
{
public:
	u32 Imm() { return U32Operand(1); }

	virtual bool CanCodegen(class Codegen *codegen, u32 pc) override;
	virtual void DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock) override;
};