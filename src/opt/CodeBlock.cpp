#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/Support/raw_ostream.h>
#include <Target/ARM/MCTargetDesc/ARMMCTargetDesc.h>
#include <llvm/MC/MCInstrInfo.h>
#include <iostream>
#include <llvm/Support/raw_os_ostream.h>
#include "CodeBlock.h"
#include "Codegen.h"
#include "core/mem_map.h"
#include "Instructions.h"

using namespace llvm;
using namespace std;

CodeBlock::CodeBlock(Codegen *codegen) : codegen(codegen), instructionCount(0)
{
	//auto x = Instructions::Read(0xe1a0400e);
	//__debugbreak();
}

CodeBlock::~CodeBlock()
{
}

u32 CodeBlock::Run(u32 start, u32 end, bool *generated)
{
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
			codegen->Write(Register::PC, codegen->irBuilder->getInt32(pc));
			codegen->irBuilder->CreateBr(codegen->outOfCodeblock);
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
		return false;
	}

	auto instr = Instructions::Read(bytes);
	if (!instr) return false;
	if (!instr->CanCodegen(codegen)) return false;

	auto basicBlock = BasicBlock::Create(*codegen->nativeContext, "", codegen->function.get());

	if (lastBlock) codegen->irBuilder->CreateBr(basicBlock);

	codegen->irBuilder->SetInsertPoint(basicBlock);
	instr->DoCodegen(codegen);

	lastBlock = basicBlock;
	codegen->RegisterBasicBlock(basicBlock, pc);

	return true;
}