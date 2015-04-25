#pragma once
#include <utility>
#include <common/common_types.h>
#include "Instruction.h"

namespace llvm { class Value;  }

enum class SRType { LSL, LSR, ASR, RRX, ROR };

class Decoder
{
private:
	class Codegen *codegen;
public:
	struct ResultCarry
	{
		llvm::Value *result, *carry;
	};
	struct ResultCarryOverflow
	{
		llvm::Value *result, *carry, *overflow;
	};
	struct ShiftTN
	{
		SRType type;
		llvm::Value *amount;
	};

	Decoder::Decoder(Codegen *codegen);
	ShiftTN DecodeImmShift(u32 type, u32 imm5);
	SRType DecodeRegShift(u32 type);
	llvm::Value *Shift32(llvm::Value *value, SRType type, llvm::Value *amount, llvm::Value *carry);

	ResultCarry Shift_C32(llvm::Value *value, SRType type, llvm::Value *amount, llvm::Value *carry);

	llvm::Value *LSL32(llvm::Value *value, llvm::Value *amount);
	ResultCarry LSL_C32(llvm::Value *value, llvm::Value *amount);

	llvm::Value *LSR32(llvm::Value *value, llvm::Value *amount);
	ResultCarry LSR_C32(llvm::Value *value, llvm::Value *amount);

	ResultCarry ASR_C32(llvm::Value *value, llvm::Value *amount);

	ResultCarry ROR_C32(llvm::Value *value, llvm::Value *amount);

	ResultCarry RRX_C32(llvm::Value *value, llvm::Value *carry);

	llvm::Value *ARMExpandImm(u32 imm);
	ResultCarry ARMExpandImm_C(u32 imm, llvm::Value *carry);
	ResultCarryOverflow AddWithCarry3232(llvm::Value *x, llvm::Value *y, llvm::Value *carry_in);
	void CreateConditionPassed(class CodeBlock *codeBlock, Condition cond, llvm::BasicBlock *passed, llvm::BasicBlock *notPassed);
};