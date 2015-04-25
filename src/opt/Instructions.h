#include <memory>
#include "Instruction.h"
#include "Mov.h"
#include "Branch.h"
#include "Cmp.h"
#include "Ldr.h"

template<typename T>
class class_wrapper { };

template<typename... Instructions>
class InstructionsBase
{
public:
	static std::unique_ptr<InstructionBase> Read(u32 inst)
	{
		return std::unique_ptr<InstructionBase>(ReadBase(inst, construct<class_wrapper<Instructions>>()...));
	}
private:
	static InstructionBase *ReadBase(u32 inst) { return nullptr; }
	template<typename Inst, typename... Rest>
	static InstructionBase *ReadBase(u32 inst, class_wrapper<Inst>, Rest... rest)
	{
		if (Inst::Check(inst))
		{
			auto r = new Inst();
			r->Read(inst);
			return r;
		}
		return ReadBase(inst, rest...);
	}
};

#define INSTRUCTIONS Mov, Branch, Cmp, Ldr

extern template InstructionsBase<INSTRUCTIONS>;

using Instructions = InstructionsBase<INSTRUCTIONS>;

// Hottest: STR ADD BBL BX CMP=8% CPY LDM SXTH LDR UXTH LDRB MOV MVN ORR RSB RSC STM STR UXTB STRB SUB SWI TST