#pragma once
#include "Instruction.h"

namespace LdrForms
{
	enum { PC, Reg, LdrMultiReg, StrMultiReg, StrReg };
}

class Ldr : public Instruction<
	//					0: cond                  1: U							2: Rt	  3: Imm12
	Form<LdrForms::PC , Field<4>, Bits<0,1,0,1>, Field<1>, Bits<0,0,1,1,1,1,1>, Field<4>, Field<12>>,
	//					0: cond	               1: P      2: U			    3: W      4: Read   5: Rn     6: Rt     7: Imm12
	Form<LdrForms::Reg, Field<4>, Bits<0,1,0>, Field<1>, Field<1>, Bits<0>, Field<1>, Field<1>, Field<4>, Field<4>, Field<12>>,
	//                          0: cond                           1: W               2: Rn     3: regs
	Form<LdrForms::LdrMultiReg, Field<4>, Bits<1, 0, 0, 0, 1, 0>, Field<1>, Bits<1>, Field<4>, Field<16>>,
	//                          0: cond                           1: W               2: Rn     3: regs
	Form<LdrForms::StrMultiReg, Field<4>, Bits<1, 0, 0, 1, 0, 0>, Field<1>, Bits<0>, Field<4>, Field<16>>
>
{
public:
	// PC, Reg
	u32 U() { return U32Operand(Form() == LdrForms::PC ? 1 : 2); }
	Register Rt(){ return RegOperand(Form() == LdrForms::PC ? 2 : 6); }
	u32 Imm12() { return U32Operand(Form() == LdrForms::PC ? 3 : 7); }

	// Reg, LdrMultiReg, StrMultiReg
	u32 W() { return U32Operand(Form() == LdrForms::Reg ? 3 : 1); }
	Register Rn(){ return RegOperand(Form() == LdrForms::Reg ? 5 : 2); }

	// MultiReg, StrMultiReg
	std::bitset<16> Regs() { return BitsOperand<16>(3); }

	// Reg
	u32 P() { return U32Operand(1); }
	u32 IsRead() { return U32Operand(4); }

	virtual bool CanCodegen(class Codegen *codegen, u32 pc) override;
	virtual bool IsDisabled(class Codegen* codegen, u32 pc) override;
	virtual void DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock) override;
};