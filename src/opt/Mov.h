#pragma once
#include "Instruction.h"

namespace MovForms
{
	enum{ Imm0, Imm1, Reg, RegSReg };
};

class Mov : public Instruction<
	//                   0: Cond                               1: S                     2: Rd     3: Imm12
	Form<MovForms::Imm0, Field<4>, Bits<0,0,1>, Bits<1,1,0,1>, Field<1>, Bits<0,0,0,0>, Field<4>, Field<12>>,
	//                   0: Cond                                         1: Imm4        2: Rd     3: Imm12
	Form<MovForms::Imm1, Field<4>, Bits<0,0,1>, Bits<1,0,0,0>, Bits<0>,  Field<4>,      Field<4>, Field<12>>,
	//                   0: Cond                               1: S                     2: Rd     3: Imm5   4: Op2             5: Rm
	Form<MovForms::Reg , Field<4>, Bits<0,0,0>, Bits<1,1,0,1>, Field<1>, Bits<0,0,0,0>, Field<4>, Field<5>, Field<2>, Bits<0>, Field<4>>
>
{
public:
	enum class Operation2 : u32
	{
		LSL, LSR, ASR, ROR
	};

	// Imm0, Imm1, Reg
	Register Rd() { return RegOperand(2); }

	// Imm0, Reg
	bool S() { return BoolOperand(1); }

	// Imm0, Imm1
	u32 Imm12() { return U32Operand(3); }

	// Reg
	u32 Imm5() { return U32Operand(3); }
	Operation2 Op2() { return (Operation2)U32Operand(4); }
	Register Rm() { return RegOperand(5); }

	// Imm1
	u32 Imm4() { return U32Operand(1); }

	virtual bool CanCodegen(class Codegen *codegen, u32 pc) override;
	virtual void DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock) override;
	virtual bool IsDisabled(class Codegen *codegen) override;
};