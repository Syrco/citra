#pragma once
#include "Instruction.h"

namespace ArithForms
{
	enum{ Imm, Reg, RegSReg };
};

class Arith : public Instruction<
	//                    0: Cond                1: Op     2: S      3: Rn     4: Rd     5: Imm12
	Form<ArithForms::Imm, Field<4>, Bits<0,0,1>, Field<4>, Field<1>, Field<4>, Field<4>, Field<12>>
>
{
public:
	enum class Op
	{
		BitAnd, BitXor, Sub, RSub, Add, AddC, SubC, RSubC,
		BitOr = 0xc, Move, BitClear, BitNot
	};

	Op Operation() { return (Op)U32Operand(1); }
	u32 S() { return U32Operand(2); }
	Register Rn() { return RegOperand(3); }
	Register Rd() { return RegOperand(4); }
	u32 Imm12() { return U32Operand(5); }

	virtual bool CanCodegen(class Codegen *codegen, u32 pc) override;
	virtual void DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock) override;
	virtual bool IsDisabled(class Codegen *codegen, u32 pc) override;
};