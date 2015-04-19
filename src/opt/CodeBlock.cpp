#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/Support/raw_ostream.h>
#include <Target/ARM/MCTargetDesc/ARMMCTargetDesc.h>
#include <iostream>
#include "CodeBlock.h"
#include "Codegen.h"
#include "core/mem_map.h"

using namespace llvm;
using namespace std;

CodeBlock::CodeBlock(Codegen *codegen) : codegen(codegen), instructionCount(0)
{
}

CodeBlock::~CodeBlock()
{
	basicBlock->eraseFromParent();
	function->removeFromParent();
}

u32 CodeBlock::Run(u32 start, u32 end, bool *generated)
{/*
	if (function)
	{
		while (basicBlock->size())
			basicBlock->front().removeFromParent();
			
		basicBlock->removeFromParent();
		function->removeFromParent();
	}*/
	function.reset(Function::Create(codegen->codeBlockFunctionSignature, GlobalValue::LinkageTypes::InternalLinkage, "", codegen->module.get()));
	basicBlock = BasicBlock::Create(*codegen->nativeContext, "", function.get());
	codegen->irBuilder->SetInsertPoint(basicBlock);
	registers = &function->getArgumentList().front();
	registers->setName("registers");

	char name[] = "arm_00000000";
	sprintf_s(name, "arm_%08x", start);
	function->setName(name);

	this->pc = start;

	u32 pc;
	for (pc = start; pc < end; pc += 4)
	{
		if ((pc % 0x10000) == 0) cout << hex << pc << endl;
		if (AddInstruction(pc))
		{
			++instructionCount;
			continue;
		}
		*generated = instructionCount >= 1;
		if (*generated)
		{
			auto delta = CreateInt32(instructionCount * 4);
			WriteRegister(15, codegen->irBuilder->CreateAdd(ReadRegister(15), delta));
			codegen->irBuilder->CreateRetVoid();
			//function->dump();
		}


		return pc + 4;
	}

	return pc;
}

bool CodeBlock::AddInstruction(u32 pc)
{
	auto bytes = Memory::Read32(pc);
	if (bytes == 0)
	{
		//cout << "Zeroes at " << hex << pc << endl;
		return false;
	}

	ArrayRef<uint8_t> bytesRef((uint8_t *)&bytes, 4);

	MCInst instruction;
	size_t instructionSize;
	auto status = codegen->armDisassembler->getInstruction(instruction, instructionSize, bytesRef, pc, nulls(), nulls());
	if (status != MCDisassembler::DecodeStatus::Success) return false;

	auto opcode = instruction.getOpcode();

	switch (opcode)
	{
#define OPCODE(name) case ARM::name: return Parse##name(instruction, pc);

		OPCODE(MOVr);

#undef OPCODE
		default:
			return false;
	}
}

bool CodeBlock::ParseMOVr(MCInst &instruction, u32 pc)
{
	auto opr0 = instruction.getOperand(0);
	auto opr1 = instruction.getOperand(1);

	if (!opr0.isReg() || !opr1.isReg()) return false;

	auto reg1 = ReadRegister(opr1);
	if (!reg1) return false;
	return WriteRegister(opr0, reg1) != nullptr;
}

llvm::Value *CodeBlock::GetRegisterAddress(llvm::MCOperand &operand)
{
	auto llvmNum = operand.getReg();
	u32 num = 0;
	if (llvmNum >= ARM::R0 && llvmNum <= ARM::R12) num = llvmNum - ARM::R0;
	else if (llvmNum == ARM::SP) num = 13;
	else if (llvmNum == ARM::PC) num = 13;
	else return nullptr;
	return GetRegisterAddress(num);
}
llvm::Value *CodeBlock::CreateInt32(u32 num)
{
	return codegen->irBuilder->getInt32(num);
}
llvm::Value *CodeBlock::GetRegisterAddress(u32 num)
{
	return codegen->irBuilder->CreateGEP(registers, CreateInt32(num));
}
Value *CodeBlock::ReadRegister(MCOperand &operand)
{
	auto reg = GetRegisterAddress(operand);
	if (!reg) return nullptr;
	return codegen->irBuilder->CreateLoad(reg);
}
Value *CodeBlock::WriteRegister(MCOperand &operand, Value *value)
{
	auto reg = GetRegisterAddress(operand);
	if (!reg) return nullptr;
	return codegen->irBuilder->CreateStore(value, reg);
}
llvm::Value *CodeBlock::ReadRegister(u32 num)
{
	auto reg = GetRegisterAddress(num);
	if (!reg) return nullptr;
	return codegen->irBuilder->CreateLoad(reg);
}
llvm::Value *CodeBlock::WriteRegister(u32 num, llvm::Value *value)
{
	auto reg = GetRegisterAddress(num);
	if (!reg) return nullptr;
	return codegen->irBuilder->CreateStore(value, reg);
}