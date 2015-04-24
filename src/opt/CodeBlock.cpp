#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/Support/raw_ostream.h>
#include <Target/ARM/MCTargetDesc/ARMMCTargetDesc.h>
#include <llvm/MC/MCInstrInfo.h>
#include <iostream>
#include <llvm/Support/raw_os_ostream.h>
#include <sstream>
#include "CodeBlock.h"
#include "Codegen.h"
#include "core/mem_map.h"
#include "Instructions.h"

using namespace llvm;
using namespace std;

CodeBlock::CodeBlock(Codegen *codegen, u32 pc) : codegen(codegen), pc(pc)
{
	fill(registers.begin(), registers.end(), nullptr);
	fill(registersPhi.begin(), registersPhi.end(), nullptr);
	//fill(conditionalStores.begin(), conditionalStores.end(), nullptr);
}

CodeBlock::~CodeBlock()
{
}

BasicBlock *CodeBlock::NewInstructionBasicBlock(const char *str)
{
	stringstream ss;
	ss << "block_" << hex << pc << "_" << str;
	return BasicBlock::Create(*codegen->nativeContext, ss.str(), codegen->function.get());
}
bool CodeBlock::AddInstruction()
{
	auto bytes = Memory::Read32(pc);
	if (bytes == 0) return false;

	auto instr = Instructions::Read(bytes);
	if (!instr) return false;
	if (!instr->CanCodegen(codegen, pc)) return false;
	disabled = instr->IsDisabled(codegen);

	stringstream ss;
	ss << "block_" << hex << pc;
	if (disabled) ss << "_disabled";
	basicBlock.reset(BasicBlock::Create(*codegen->nativeContext, ss.str(), codegen->function.get()));
	ss << "_loadblock";
	loadBlock.reset(BasicBlock::Create(*codegen->nativeContext, ss.str(), codegen->function.get()));

	codegen->irBuilder->SetInsertPoint(loadBlock.get());
	codegen->irBuilder->CreateBr(basicBlock.get());

	codegen->irBuilder->SetInsertPoint(basicBlock.get());
	if (!disabled)
	{
		instr->DoCodegen(codegen, this);
	}
	lastBlock = codegen->irBuilder->GetInsertBlock();

	return true;
}
llvm::Value *CodeBlock::Read(Register reg)
{
	if (!registers[(int)reg])
	{
		if (registersPhi[(int)reg]) __debugbreak();
		auto insertPoint = codegen->irBuilder->saveIP();

		codegen->irBuilder->SetInsertPoint(loadBlock.get(), loadBlock->begin());
		auto load = codegen->Read(reg);

		codegen->irBuilder->SetInsertPoint(basicBlock.get(), basicBlock->begin());
		stringstream ss;
		ss << "r" << (int)reg << "_";
		auto phi = codegen->irBuilder->CreatePHI(codegen->RegType(reg), prevs.size() + 1, ss.str().c_str());
		codegen->irBuilder->restoreIP(insertPoint);

		registers[(int)reg] = phi;
		registersPhi[(int)reg] = phi;
		phi->addIncoming(load, loadBlock.get());
		for (auto prev : prevs)
		{
			if (phi->getType() != prev->Read(reg)->getType())
			{
				prev->Read(reg)->dump();
				phi->dump();
			}
			phi->addIncoming(prev->Read(reg), prev->lastBlock);
		}
	}
	return registers[(int)reg];
}
llvm::Value *CodeBlock::Write(Register reg, llvm::Value *value)
{
	return registers[(int)reg] = value;
}
void CodeBlock::Link(CodeBlock *prev, CodeBlock *next)
{
	auto codegen = prev->codegen;
	next->prevs.push_back(prev);
	next->nexts.push_back(next);

	/*if (next->pc == 0x1001e0)
	{
		next->basicBlock->dump();
	}*/

	for (auto i = 0; i < prev->registers.size(); ++i)
	{
		auto prevR = prev->registers[i];
		auto nextR = next->registers[i];
		if (!nextR)
		{
			next->Read((Register)i);
		}
		else
		{
			if (next->registersPhi[i])
				next->registersPhi[i]->addIncoming(prev->Read((Register)i), prev->lastBlock);
		}
	}

	/*if (next->pc == 0x1001e0)
	{
		next->basicBlock->dump();
		__debugbreak();
	}*/

	if (prev->lastBlock->getTerminator()) __debugbreak();
	codegen->irBuilder->SetInsertPoint(prev->lastBlock);
	codegen->irBuilder->CreateBr(next->basicBlock.get());
}
void CodeBlock::Terminate()
{
	TerminateAt(pc + 4);
}
void CodeBlock::JumpFailed()
{
	TerminateAt(jumpAddress);
}
void CodeBlock::Spill(int reg)
{
	//if (this->pc == 0x114d2c && reg == 14) __debugbreak();
	auto val = registers[reg];
	if (!val) return;
	if (val == registersPhi[reg])
	{
		for (auto prev : prevs)
		{
			prev->Spill(reg);
		}
	}
	else
	{
		codegen->irBuilder->SetInsertPoint(lastBlock, --lastBlock->end());
		codegen->Write((Register)reg, val);
	}
}
void CodeBlock::TerminateAt(u32 pc)
{
	if (lastBlock->getTerminator()) __debugbreak();
	codegen->irBuilder->SetInsertPoint(lastBlock);
	codegen->Write(Register::PC, codegen->irBuilder->getInt32(pc));
	codegen->irBuilder->CreateBr(codegen->outOfCodeblock);

	for (auto i = 0; i < registers.size(); ++i)
	{
		Spill(i);
	}
}
void CodeBlock::BeginCond(Condition cond)
{
	condPassed = BasicBlock::Create(*codegen->nativeContext, "Passed", codegen->function.get());;
	condNotPassed = BasicBlock::Create(*codegen->nativeContext, "NotPassed", codegen->function.get());
	codegen->decoder->CreateConditionPassed(this, cond, condPassed, condNotPassed);

	codegen->irBuilder->SetInsertPoint(condPassed);
	inConditionalBlock = true;
}
void CodeBlock::EndCond()
{
	codegen->irBuilder->CreateBr(condNotPassed);
	codegen->irBuilder->SetInsertPoint(condNotPassed);

	for (auto i = 0; i < registers.size(); ++i)
	{
		auto newVal = registers[i];
		auto oldPhi = registersPhi[i];
		if (newVal == oldPhi) continue;

		registers[i] = nullptr;
		auto oldVal = oldPhi ? oldPhi : Read((Register)i);

		auto newPhi = codegen->irBuilder->CreatePHI(newVal->getType(), 2);
		newPhi->addIncoming(newVal, condPassed);
		newPhi->addIncoming(oldVal, basicBlock.get());
		Write((Register)i, newPhi);
	}
}