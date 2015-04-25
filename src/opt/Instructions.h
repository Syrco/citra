#include <memory>
#include "Instruction.h"
#include "Instructions/Mov.h"
#include "Instructions/Branch.h"
#include "Instructions/Cmp.h"
#include "Instructions/Ldr.h"
#include "Instructions/Arith.h"

template<typename T>
class class_wrapper { };

template<typename... Instructions>
class InstructionsBase
{
public:
	static std::unique_ptr<InstructionBase> Read(class Codegen *codegen, u32 pc, u32 inst)
	{
		return std::unique_ptr<InstructionBase>(ReadBase(codegen, pc, inst, construct<class_wrapper<Instructions>>()...));
	}
private:
	static InstructionBase *ReadBase(class Codegen *codegen, u32 pc, u32 inst) { return nullptr; }
	template<typename Inst, typename... Rest>
	static InstructionBase *ReadBase(class Codegen *codegen, u32 pc, u32 inst, class_wrapper<Inst>, Rest... rest)
	{
		if (Inst::Check(inst))
		{
			auto r = new Inst();
			r->Read(inst);
			if (!r->CanCodegen(codegen, pc))
				delete r;
			else
				return r;
		}
		return ReadBase(codegen, pc, inst, rest...);
	}
};

#define INSTRUCTIONS Mov, Branch, Cmp, Ldr, Arith

extern template InstructionsBase<INSTRUCTIONS>;

using Instructions = InstructionsBase<INSTRUCTIONS>;

// Hottest: STR ADD BBL BX CMP=8% CPY LDM SXTH LDR UXTH LDRB MOV MVN ORR RSB RSC STM STR UXTB STRB SUB SWI TST