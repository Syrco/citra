#pragma once
#include <common/common_types.h>
#include <algorithm>
#include <iostream>
#include <bitset>

namespace llvm
{
	class BasicBlock;
}

//template<typename T> struct construct { T value(){ return T() }; };
template<typename T> T construct(){ return T(); }

template<u32...> class Bits { };
template<u32 Size> class Field { };

template<size_t FormN, typename... Args>
class Form
{
public:
	static const u32 OperandCount = sizeof...(Args);
	static const size_t FormID = FormN;

	static bool Read(u32 inst, u32 *operands = nullptr)
	{
		return ReadBase(0, inst, operands, construct<Args>()...);
	}
private:

	static bool ReadBase(u32 bitCount, u32 inst, u32 *operands) { if (bitCount != 32) __debugbreak(); return true; }

	template<typename... Rest>
	static bool ReadBase(u32 bitCount, u32 inst, u32 *operands, Bits<>, Rest... rest) { return ReadBase(bitCount, inst, operands, rest...); }

	template<u32 Bit, u32... BitsRest, typename... Rest>
	static bool ReadBase(u32 bitCount, u32 inst, u32 *operands, Bits<Bit, BitsRest...>, Rest... rest)
	{
		//std::cout << "ReadBase bit " << std::hex << inst << " Got " << (inst >> 31) << " wanted " << Bit << std::endl;
		if (!ReadBase(bitCount + 1, inst << 1, operands, Bits<BitsRest...>(), rest...)) return false;
		return (inst >> 31) == Bit;
	}

	template<u32 Length, typename... Rest>
	static bool ReadBase(u32 bitCount, u32 inst, u32 *operands, Field<Length>, Rest... rest)
	{
		//std::cout << "ReadBase " << Length << " " << std::hex << inst << std::endl;
		if (!ReadBase(bitCount + Length, inst << Length, operands ? operands + 1 : nullptr, rest...)) return false;
		if (operands) operands[0] = inst >> (32 - Length);
		return true;
	}

};

enum class Condition
{
	EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, Invalid
};

enum class Register
{
	R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, SP, LR, PC,
	N, Z, C, V,
	Count
};

class InstructionBase
{
protected:
public:
	virtual ~InstructionBase();
	virtual bool Read(u32 inst) = 0;
	virtual bool IsDisabled(class Codegen* codegen, u32 pc);
	virtual bool CanCodegen(class Codegen *codegen, u32 pc) = 0;
	virtual void DoCodegen(class Codegen *codegen, class CodeBlock *codeBlock) = 0;
};

template<typename... Forms>
class Instruction : public InstructionBase
{
	//template<size_t...> struct max;
	//template<size_t N> struct max<N> : public std::integral_constant < size_t, N >{};
	//template<size_t N, size_t... M>	struct max<N, M...>
	//	: public std::integral_constant < size_t, max<N, max<M...>::value>::value >{};
	template<size_t N, size_t... Rest> struct max : std::integral_constant<size_t, (max<Rest...>::value > N ? max<Rest...>::value : N)> { };
	template<size_t N> struct max<N> : std::integral_constant < size_t, N > { };

	u32 form;
	u32 operands[max<Forms::OperandCount...>::value];

	static bool ReadBase(u32 inst, Instruction *result)
	{
		return false;
	}

	template<typename Form, typename... Rest>
	static bool ReadBase(u32 inst, Instruction *result, Form, Rest... rest)
	{
		if (!Form::Read(inst, result ? result->operands : nullptr)) return ReadBase(inst, result, rest...);
		if (result) result->form = Form::FormID;
		return true;
	}

protected:

	bool BoolOperand(size_t i) { return operands[i] ? true : false; }
	u32 U32Operand(size_t i) { return (u32)operands[i]; }
	Condition CondOperand(size_t i) { return (Condition)operands[i]; }
	Register RegOperand(size_t i) { return (Register)operands[i]; }
	template<size_t N>
	std::bitset<N> BitsOperand(size_t i) { return std::bitset<N>(U32Operand(i)); }
	u32 Form() { return form; }

public:
	static bool Check(u32 inst)
	{
		return ReadBase(inst, nullptr, construct<Forms>()...);
	}
	virtual bool Read(u32 inst)
	{
		return ReadBase(inst, this, construct<Forms>()...);
	}
	Condition Cond() { return CondOperand(0); }
};