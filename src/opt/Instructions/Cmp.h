#pragma once
#include "Instruction.h"

namespace CmpForms
{
	enum { Imm, Reg, RegSReg };
};

class Cmp : public Instruction<
	Form<CmpForms::Imm,     Field<4>, Bits<0,0,1,1,0,1,0,1>, Field<4>, Bits<0,0,0,0>, Field<12>>,
	Form<CmpForms::Reg,     Field<4>, Bits<0,0,0,1,0,1,0,1>, Field<4>, Bits<0,0,0,0>, Field<5>,          Field<2>, Bits<0>, Field<4>>,
	Form<CmpForms::RegSReg, Field<4>, Bits<0,0,0,1,0,1,0,1>, Field<4>, Bits<0,0,0,0>, Field<4>, Bits<0>, Field<2>, Bits<1>, Field<4>>
>
{
public:
	// All forms
	Register Rn() { return RegOperand(1); }

	// Imm
	u32 Imm12() { return U32Operand(2); }

	// Reg
	u32 Imm5() { return U32Operand(2); }

	// RegSReg
	Register Rs() { return RegOperand(2); }

	// Reg, RegSReg
	u32 Type() { return U32Operand(3); }
	Register Rm() { return RegOperand(4); }

	virtual bool CanCodegen(class Codegen *codegen, u32 pc) override;
	virtual bool IsDisabled(class Codegen* codegen, u32 pc) override;
	virtual void DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock) override;
};