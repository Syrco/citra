#include "Jit.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/Support/Host.h>
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include <Target/ARM/MCTargetDesc/ARMMCTargetDesc.h>
#include <core/mem_map.h>
#include <core/arm/skyeye_common/armdefs.h>
#include <core/arm/dyncom/arm_dyncom_thumb.h>

using namespace llvm;
using namespace std;

#pragma optimize( "", off )

struct Jit::Private
{
	Jit::Private();

	llvm::ExecutionEngine *executionEngine;
	IRBuilder<> *builder;
	const Target *armTarget;
	MCSubtargetInfo *armSubtarget;
	const MCDisassembler *armDisassembler;
	const MCRegisterInfo *armRegisterInfo;
	const MCAsmInfo *armAsmInfo;
	MCContext *armContext;
	Module *module;

	struct ARMul_State *state;

	vector<unique_ptr<class JitBlock>> jitBlocks;

	bool InterpreterTranslate(struct ARMul_State* cpu, u32 addr);
};

struct JitBlock
{
	Jit::Private *jit;

	JitBlock(Jit::Private *jit);
	~JitBlock();

	struct Register
	{
		Value *value;
		bool modified;
	};

	Function *function;
	std::map<int, Register> registers;

	u32 *GetRegisterPtr(int reg);
	Value *GetConstPtr(void *ptr);
	Value *ReadRegister(int reg);
	bool WriteRegister(int reg, Value *value);
	bool AddInstruction(u32 *addr);
	void Finalize();
};

Jit::Jit()
{

	priv = std::make_unique<Private>();
}

Jit::Private::Private()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();

	LLVMInitializeARMTargetInfo();
	LLVMInitializeARMTarget();
	LLVMInitializeARMTargetMC();
	LLVMInitializeARMAsmPrinter();
	LLVMInitializeARMAsmParser();
	LLVMInitializeARMDisassembler();

	auto module = llvm::make_unique<Module>("Module", getGlobalContext());
	module->setTargetTriple(sys::getProcessTriple() + "-elf");
	this->module = module.get();
	std::string errorString;
	executionEngine = EngineBuilder(std::move(module))
		.setErrorStr(&errorString)
		.setMCJITMemoryManager(llvm::make_unique<SectionMemoryManager>())
		.create();
	if (!executionEngine) __debugbreak();

	builder = new IRBuilder<>(getGlobalContext());

	StringRef triple = "armv6k-none-eabi";

	armTarget = TargetRegistry::lookupTarget(triple, errorString);
	if (!armTarget) __debugbreak();

	armSubtarget = armTarget->createMCSubtargetInfo(triple, "armv6k", "");
	if (!armSubtarget) __debugbreak();

	armRegisterInfo = armTarget->createMCRegInfo(triple);
	if (!armRegisterInfo) __debugbreak();

	armAsmInfo = armTarget->createMCAsmInfo(*armRegisterInfo, triple);
	if (!armAsmInfo) __debugbreak();

	armContext = new MCContext(armAsmInfo, armRegisterInfo, nullptr);

	armDisassembler = armTarget->createMCDisassembler(*armSubtarget, *armContext);
	if (!armDisassembler) __debugbreak();
}

bool Jit::InterpreterTranslate(ARMul_State* state, u32 addr)
{
	return priv->InterpreterTranslate(state, addr);
}

bool Jit::Private::InterpreterTranslate(ARMul_State *state, u32 addr)
{
	this->state = state;

	if (state->TFlag) return false;

	auto jb = new JitBlock(this);

	for (size_t instructionCount = 0;; ++instructionCount)
	{
		if (jb->AddInstruction(&addr)) continue;
		if (!instructionCount)
		{
			delete jb;
			return false;
		}
		auto pc = jb->GetConstPtr(&state->Reg[15]);
		auto delta = ConstantInt::getIntegerValue(Type::getInt32Ty(getGlobalContext()), APInt(32, 4 * instructionCount));
		auto add = builder->CreateAdd(builder->CreateLoad(pc), delta);
		builder->CreateStore(add, pc);
		verifyFunction(*jb->function);
		jb->Finalize();
		module->dump();
		executionEngine->finalizeObject();
		void *f = executionEngine->getPointerToFunction(jb->function);

		jb->function->dump();

		auto fPtr = (void(*)())f;
		__debugbreak();
		fPtr();
		return true;
	}
}

JitBlock::JitBlock(Jit::Private *jit) : jit(jit)
{
	FunctionType *ft = FunctionType::get(Type::getVoidTy(getGlobalContext()), false);

	function = Function::Create(ft, Function::ExternalLinkage, "", jit->module);
	BasicBlock *bb = BasicBlock::Create(getGlobalContext(), "", function);
	jit->builder->SetInsertPoint(bb);
}

JitBlock::~JitBlock()
{
	function->removeFromParent();
}

u32 *JitBlock::GetRegisterPtr(int reg)
{
	u32 *addr = nullptr;
	if (reg >= ARM::R0 && reg <= ARM::R12) return &jit->state->Reg[reg - ARM::R0];
	else if (reg == ARM::SP) return &jit->state->Reg[13];
	else if (reg == ARM::LR) return &jit->state->Reg[14];
	return nullptr;
}
Value *JitBlock::GetConstPtr(void *ptr)
{
	auto bits = sizeof(size_t) * 8;
	//return Constant::getIntegerValue(Type::getIntNPtrTy(getGlobalContext(), bits), APInt(bits, (uint64_t)ptr));
	//auto type = Type::getInt32Ty(getGlobalContext());
	//auto value = new GlobalVariable(*jit->module, type, false,
	//	GlobalValue::LinkageTypes::InternalLinkage, nullptr, "register");
	//jit->executionEngine->addGlobalMapping(value, ptr);
	//return value;
	return jit->builder->CreateIntToPtr(ConstantInt::getIntegerValue(Type::getIntNTy(getGlobalContext(), bits), APInt(bits, (uint64_t)ptr)),
		Type::getInt32PtrTy(getGlobalContext()));
}

Value *JitBlock::ReadRegister(int reg)
{
	auto i = registers.find(reg);
	if (i != registers.end()) return i->second.value;

	auto regPtr = GetRegisterPtr(reg);
	if (!regPtr) return nullptr;

	auto value = jit->builder->CreateLoad(GetConstPtr(regPtr));
	registers[reg] = { value, false };
	return value;
}

bool JitBlock::WriteRegister(int reg, Value *value)
{
	if (!GetRegisterPtr(reg)) return false;
	registers[reg] = { value, true };
	return true;
}

bool JitBlock::AddInstruction(u32 *addr)
{
	auto inst = Memory::Read32(*addr & 0xFFFFFFFC);

	MCInst instruction;
	size_t instructionSize;
	auto bytes = ArrayRef<uint8_t>((uint8_t *)&inst, 4);
	auto status = jit->armDisassembler->getInstruction(instruction, instructionSize, bytes, *addr & 0xFFFFFFFC, nulls(), nulls());
	*addr += instructionSize;

	auto opcode = instruction.getOpcode();

	LOG_INFO(Core_ARM11, "Status %x, Opcode %x", status, opcode);

	switch (opcode)
	{
		case ARM::MOVr:
		{
			auto opr0 = instruction.getOperand(0);
			auto opr1 = instruction.getOperand(1);

			if (!opr1.isReg()) return false;

			auto reg0 = opr0.getReg();
			auto reg1 = ReadRegister(opr1.getReg());
			if (!reg1) return false;
			if (!WriteRegister(reg0, reg1)) return false;
			break;
		}
		default: return false;
	}

	return true;
}
void JitBlock::Finalize()
{
	for (auto i : registers)
	{
		auto num = i.first;
		auto reg = i.second;
		if (!reg.modified) continue;

		jit->builder->CreateStore(reg.value, GetConstPtr(GetRegisterPtr(num)));
	}

	jit->builder->CreateRetVoid();
}