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
	return BasicBlock::Create(*codegen->nativeContext, ss.str());
}
bool CodeBlock::AddInstruction()
{
	auto bytes = Memory::Read32(pc);
	if (bytes == 0) return false;

	auto instr = Instructions::Read(codegen, pc, bytes);
	if (!instr) return false;
	disabled = instr->IsDisabled(codegen, pc);

	stringstream ss;
	ss << "block_" << hex << pc;
	if (disabled) ss << "_disabled";
	basicBlock.reset(BasicBlock::Create(*codegen->nativeContext, ss.str()));
	ss << "_loadblock";
	loadBlock.reset(BasicBlock::Create(*codegen->nativeContext, ss.str()));

	codegen->irBuilder->SetInsertPoint(loadBlock.get());
	codegen->irBuilder->CreateBr(basicBlock.get());

	codegen->irBuilder->SetInsertPoint(basicBlock.get());
	if (!disabled)
	{
		instr->DoCodegen(codegen, this);
	}
	lastBlock = codegen->irBuilder->GetInsertBlock();
	/*if (wrotePC)
	{
		codegen->irBuilder->SetInsertPoint(lastBlock);
		codegen->Write(Register::PC, pcValue);
		//auto call = codegen->irBuilder->CreateCall(codegen->entryFunction.get());
		//call->setTailCall(true);
		codegen->irBuilder->CreateRetVoid();

		for (auto i = 0; i < registers.size(); ++i)
		{
			Spill(i);
		}
	}*/

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
	prev->nexts.push_back(next);

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
		auto pt = lastBlock->end();
		if (lastBlock->getTerminator()) --pt;
		if (wrotePC) --pt;
		codegen->irBuilder->SetInsertPoint(lastBlock, pt);
		codegen->Write((Register)reg, val);
	}
}
void CodeBlock::TerminateAt(u32 pc)
{
	if (lastBlock->getTerminator()) __debugbreak();
	codegen->irBuilder->SetInsertPoint(lastBlock);
	codegen->Write(Register::PC, codegen->irBuilder->getInt32(pc));
	//codegen->irBuilder->CreateBr(codegen->outOfCodeblock);

	for (auto i = 0; i < registers.size(); ++i)
	{
		Spill(i);
	}
}
void CodeBlock::TerminateWritePC()
{
	if (lastBlock->getTerminator()) __debugbreak();
	codegen->irBuilder->SetInsertPoint(lastBlock);
	//codegen->Write(Register::PC, pcValue);
	auto call = codegen->irBuilder->CreateCall(codegen->reentryFunction.get(), pcValue);
	call->setTailCall(true);
	codegen->irBuilder->CreateRetVoid();

	for (auto i = 0; i < registers.size(); ++i)
	{
		Spill(i);
	}
}
void CodeBlock::WritePC(llvm::Value *value)
{
	wrotePC = true;
	pcValue = value;
}
void CodeBlock::BeginCond(Condition cond)
{
	if (cond == Condition::AL)
	{
		condNotPassed = nullptr;
		return;
	}

	condPassed = BasicBlock::Create(*codegen->nativeContext, "Passed");
	condNotPassed = BasicBlock::Create(*codegen->nativeContext, "NotPassed");
	codegen->decoder->CreateConditionPassed(this, cond, condPassed, condNotPassed);

	codegen->irBuilder->SetInsertPoint(condPassed);
	inConditionalBlock = true;
}
void CodeBlock::EndCond()
{
	if (!condNotPassed) return;
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