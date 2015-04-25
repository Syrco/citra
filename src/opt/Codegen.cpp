#include "Codegen.h"
#include "CodeBlock.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCInstPrinter.h>
#include <llvm/Target/TargetSubtargetInfo.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/PassManager.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <core/mem_map.h>
#include <sstream>
#include "common/logging/text_formatter.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "Instructions.h"
#include <iostream>
#include "Decode.h"

using namespace llvm;
using namespace std;

Codegen::Codegen() : decoder(new Decoder(this))
{
	// Native
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	nativeContext.reset(new LLVMContext());

	auto nativeTripleStr = sys::getProcessTriple();
#ifdef _WIN32
	nativeTripleStr += "-elf";
#endif
	nativeTriple.setTriple(nativeTripleStr);
	EngineBuilder eb(llvm::make_unique<Module>("Decoy module", *nativeContext));
	nativeTarget.reset(eb.selectTarget(nativeTriple, "", "", llvm::SmallVector<std::string, 0>()));
	if (!nativeTarget) __debugbreak();

	module = llvm::make_unique<Module>("Module", *nativeContext);
	module->setTargetTriple(nativeTripleStr);
	irBuilder = llvm::make_unique<IRBuilder<>>(*nativeContext);
}

Codegen::~Codegen()
{

}

const size_t Codegen::TranslateSize = 0x24000;
//const size_t Codegen::TranslateSize = 0x40;
//static const size_t TranslateSize = 0x13AD0;
//static const size_t TranslateSize = BinSearch(0x25000).r().r().r().r().l().l().r().r().l().l().l().l().mid;
//static const size_t TranslateSize = BinSearch(0x25000).r().r().r().r().l().l().r().r().l().l().l().l().mid + 4;


const size_t Codegen::TranslateStart = Memory::EXEFS_CODE_VADDR;
const size_t Codegen::TranslateEnd = TranslateStart + TranslateSize;

void Codegen::Run(const char *filename)
{
	GenerateMetadata();
	GenerateGlobals();
	TranslateBlocks();
	ColorBlocks();
	GenerateSwitchArray();
	GenerateEntryOrReentryFunction(true);
	GenerateEntryOrReentryFunction(false);
	//GenerateEntryFunction();
	//GenerateReentryFunction();
	GeneratePresentFunction();

	WriteFile(filename);
}

void Codegen::GenerateMetadata()
{
	mdBuilder.reset(new MDBuilder(*nativeContext));
	auto tbaaRoot = mdBuilder->createTBAARoot("Root");
	for (auto i = 0; i < (int)Register::Count; ++i)
	{
		stringstream ss;
		ss << "Register" << i;
		mdRegisters[i] = mdBuilder->createTBAAScalarTypeNode(ss.str().c_str(), tbaaRoot);
	}
	mdRegistersGlobal = mdBuilder->createTBAAScalarTypeNode("RegistersGlobal", tbaaRoot);
	mdRead32 = mdBuilder->createTBAAScalarTypeNode("Read32", tbaaRoot);
	mdMemory = mdBuilder->createTBAAScalarTypeNode("Memory", tbaaRoot);
	mdSwitchArrays = mdBuilder->createTBAAScalarTypeNode("SwitchArrays", tbaaRoot);
}

void Codegen::GenerateGlobals()
{
	entryFunctionSignature = FunctionType::get(Type::getVoidTy(*nativeContext), false);
	codeBlockFunctionSignature = FunctionType::get(Type::getVoidTy(*nativeContext), Type::getInt8PtrTy(*nativeContext), false);
	auto reentryFunctionSignature = FunctionType::get(Type::getVoidTy(*nativeContext), irBuilder->getInt32Ty(), false);

	auto i32ptr = Type::getInt32PtrTy(*nativeContext);
	auto i1ptr = Type::getInt1PtrTy(*nativeContext);
	registersGlobal = new GlobalVariable(*module, i32ptr, false, GlobalValue::ExternalLinkage, ConstantPointerNull::get(i32ptr), "Registers", nullptr, GlobalValue::NotThreadLocal, 0, true);
	flagsGlobal = new GlobalVariable(*module, i1ptr, false, GlobalValue::ExternalLinkage, ConstantPointerNull::get(i1ptr), "Flags", nullptr, GlobalValue::NotThreadLocal, 0, true);

	auto read32Type = FunctionType::get(irBuilder->getInt32Ty(), { irBuilder->getInt32Ty() }, false);
	auto read32PtrType = PointerType::get(read32Type, 0);
	Type *write32Params[] = { irBuilder->getInt32Ty(), irBuilder->getInt32Ty() };
	auto write32Type = FunctionType::get(irBuilder->getInt32Ty(), write32Params, false);
	auto write32PtrType = PointerType::get(write32Type, 0);
	read32Global = new GlobalVariable(*module, read32PtrType, false, GlobalValue::ExternalLinkage, ConstantPointerNull::get(read32PtrType),
		"Memory::Read32", nullptr, GlobalValue::NotThreadLocal, 0, true);
	write32Global = new GlobalVariable(*module, write32PtrType, false, GlobalValue::ExternalLinkage, ConstantPointerNull::get(write32PtrType),
		"Memory::Write32", nullptr, GlobalValue::NotThreadLocal, 0, true);

	entryFunction.reset(Function::Create(entryFunctionSignature, GlobalValue::LinkageTypes::ExternalLinkage, "Run", module.get()));
	reentryFunction.reset(Function::Create(reentryFunctionSignature, GlobalValue::LinkageTypes::PrivateLinkage, "Reenter", module.get()));
}

void Codegen::ColorBlocks()
{
	size_t color = 0;

	for (auto p : blocks)
	{
		auto block = p.second;
		if (blockColors.count(block)) continue;
		colorBlocks.push_back({});
		ColorBlock(block, color);
		GenerateFunctionForColor(color);
		color++;
	}

	cout << hex << color << " block colors" << endl;
}

void Codegen::ColorBlock(CodeBlock *codeBlock, size_t color)
{
	if (blockColors.count(codeBlock)) return;
	blockColors[codeBlock] = color;
	colorBlocks[color].push_back(codeBlock);

	for (auto next : codeBlock->nexts) ColorBlock(next, color);
	for (auto prev : codeBlock->prevs) ColorBlock(prev, color);
}

void Codegen::GenerateFunctionForColor(size_t color)
{
	auto colorList = colorBlocks[color];
	stringstream ss;
	ss << "BlockColor_" << color;
	auto function = Function::Create(codeBlockFunctionSignature, GlobalValue::PrivateLinkage, ss.str().c_str(), module.get());
	blockFunction[color] = function;
	auto arg = &function->getArgumentList().front();
	auto entryBlock = BasicBlock::Create(*nativeContext, (ss.str() + "_Entry").c_str(), function);
	auto exitBlock = BasicBlock::Create(*nativeContext, (ss.str() + "_Exit").c_str(), function);
	irBuilder->SetInsertPoint(entryBlock);
	auto indirectBr = irBuilder->CreateIndirectBr(arg, colorList.size());
	for (auto block : colorList)
	{
		indirectBr->addDestination(block->loadBlock.get());
		if (!block->lastBlock->getTerminator())
		{
			irBuilder->SetInsertPoint(block->lastBlock);
			irBuilder->CreateBr(exitBlock);
		}
		InsertBlock(block->loadBlock.get(), function);
	}
	irBuilder->SetInsertPoint(exitBlock);
	irBuilder->CreateRetVoid();
}

void Codegen::InsertBlock(BasicBlock *block, Function *function)
{
	if (block->getParent())
	{
		if (block->getParent() != function)
		{
			cout << function->getName().begin() << endl;
			cout << block->getParent()->getName().begin() << endl;
			block->dump();
		}
		assert(block->getParent() == function);
		return;
	}
	block->insertInto(function);
	auto terminator = block->getTerminator();
	if (terminator)
	{
		for (auto i = 0; i < terminator->getNumSuccessors(); ++i)
		{
			InsertBlock(terminator->getSuccessor(i), function);
		}
	}
}

void Codegen::GenerateEntryOrReentryFunction(bool entry)
{
	auto function = entry ? entryFunction.get() : reentryFunction.get();

	auto switchBasicBlock = BasicBlock::Create(*nativeContext, "switch", function);
	auto noCaseBasicBlock = BasicBlock::Create(*nativeContext, "noCase", function);
	auto caseBasicBlock = BasicBlock::Create(*nativeContext, "case", function);
	auto arg = !entry ? &*function->arg_begin() : nullptr;

	irBuilder->SetInsertPoint(caseBasicBlock);

	Value *outValue;

	irBuilder->SetInsertPoint(switchBasicBlock);
	auto pc = entry ? Read(Register::PC) : arg;
	GenerateSwitchArrayIf(pc, function,
		switchBasicBlock, caseBasicBlock, noCaseBasicBlock,
		&outValue);

	irBuilder->SetInsertPoint(caseBasicBlock);
	auto block = irBuilder->CreateExtractValue(outValue, 0);
	auto func = irBuilder->CreateExtractValue(outValue, 1);
	auto call = irBuilder->CreateCall(func, block);
	call->setTailCall();
	irBuilder->CreateRetVoid();

	irBuilder->SetInsertPoint(noCaseBasicBlock);
	if (!entry)
	{
		Write(Register::PC, arg);
	}
	irBuilder->CreateRetVoid();
}

void Codegen::GeneratePresentFunction()
{
	Type *arguments = { irBuilder->getInt32Ty() };
	auto presentFunctionSignature = FunctionType::get(irBuilder->getInt1Ty(), arguments, false);
	auto presentFunction = Function::Create(presentFunctionSignature, GlobalValue::LinkageTypes::ExternalLinkage, "Present", module.get());
	auto pc = &presentFunction->getArgumentList().front();

	Value *addr;
	auto switchBasicBlock = BasicBlock::Create(*nativeContext, "switch", presentFunction);
	auto trueBasicBlock = BasicBlock::Create(*nativeContext, "true", presentFunction);
	auto falseBasicBlock = BasicBlock::Create(*nativeContext, "false", presentFunction);

	irBuilder->SetInsertPoint(switchBasicBlock);
	GenerateSwitchArrayIf(pc, presentFunction,
		switchBasicBlock, trueBasicBlock, falseBasicBlock,
		&addr);

	irBuilder->SetInsertPoint(trueBasicBlock);
	irBuilder->CreateRet(irBuilder->getInt1(true));

	irBuilder->SetInsertPoint(falseBasicBlock);
	irBuilder->CreateRet(irBuilder->getInt1(false));
}

void Codegen::GenerateSwitchArray()
{
	switchArraySize = TranslateSize / 4;	
	PointerType *structElementsTypes[] = { IntegerType::getInt8PtrTy(*nativeContext), PointerType::get(codeBlockFunctionSignature, 0) };
	switchArrayMemberType = StructType::get(*nativeContext, ArrayRef<Type*>((Type**)structElementsTypes, 2));
	auto switchArrType = ArrayType::get(switchArrayMemberType, blocks.size());
	auto switchIndirectArrayType = ArrayType::get(irBuilder->getInt32Ty(), switchArraySize);

	auto switchLocalArr = new Constant*[blocks.size()];
	auto switchIndirectLocalArray = new Constant*[switchArraySize];
	std::fill(switchIndirectLocalArray, switchIndirectLocalArray + switchArraySize, irBuilder->getInt32(-1));
	auto index = 0;
	for (auto color = 0; color < colorBlocks.size(); ++color)
	{
		for (auto block : colorBlocks[color])
		{
			Constant *structElements[] = { BlockAddress::get(block->loadBlock->getParent(), block->loadBlock.get()), block->loadBlock->getParent() };
			switchLocalArr[index] = ConstantStruct::get(switchArrayMemberType, ArrayRef<Constant*>(structElements));
			switchIndirectLocalArray[(block->pc - TranslateStart) / 4] = irBuilder->getInt32(index);
			++index;
		}
	}
	auto switchArrayConst = ConstantArray::get(switchArrType, ArrayRef<Constant*>(switchLocalArr, switchLocalArr + blocks.size()));
	auto switchIndirectArrayConst = ConstantArray::get(switchIndirectArrayType, ArrayRef<Constant*>(switchIndirectLocalArray, switchIndirectLocalArray + switchArraySize));
	delete switchLocalArr;
	switchArray = new GlobalVariable(*module, switchArrayConst->getType(), true, GlobalValue::InternalLinkage, switchArrayConst, "SwitchArray");
	switchIndirectArray = new GlobalVariable(*module, switchIndirectArrayConst->getType(), true, GlobalValue::InternalLinkage, switchIndirectArrayConst, "SwitchIndirectArray");
}

void Codegen::GenerateSwitchArrayIf(llvm::Value *offset, llvm::Function *function,
	llvm::BasicBlock *enterBasicBlock, llvm::BasicBlock *exitTrueBasicBlock, llvm::BasicBlock *exitFalseBasicBlock,
	llvm::Value **pointer)
{
	auto bb0 = enterBasicBlock;                                     // if(pc <)
	auto bb1 = BasicBlock::Create(*nativeContext, "bb1", function); //  if(value != 0)
	auto bb2 = exitTrueBasicBlock;									//   then
	auto bb3 = exitFalseBasicBlock;									// else

	irBuilder->SetInsertPoint(bb0);
	auto pc = offset;
	auto pcSubStart = irBuilder->CreateSub(pc, irBuilder->getInt32(TranslateStart));
	auto pcSubStartDiv4 = irBuilder->CreateAShr(pcSubStart, 2);
	auto higher = irBuilder->CreateICmpUGE(pcSubStartDiv4, irBuilder->getInt32(TranslateSize / 4));
	auto iff = irBuilder->CreateCondBr(higher, bb3, bb1);

	irBuilder->SetInsertPoint(bb1);
	Value *values[] = { irBuilder->getInt32(0), pcSubStartDiv4 };
	auto indirectIndex = irBuilder->CreateLoad(irBuilder->CreateInBoundsGEP(switchIndirectArray, values));
	indirectIndex->setMetadata(LLVMContext::MD_tbaa, mdSwitchArrays);
	auto cmp = irBuilder->CreateICmpNE(indirectIndex, irBuilder->getInt32(-1));
	auto iff2 = irBuilder->CreateCondBr(cmp, bb2, bb3);
	irBuilder->SetInsertPoint(bb2);
	values[1] = indirectIndex;
	auto value = irBuilder->CreateLoad(irBuilder->CreateInBoundsGEP(switchArray, values));
	value->setMetadata(LLVMContext::MD_tbaa, mdSwitchArrays);
	*pointer = value;
}

void Codegen::WriteLL(const char *filename)
{
	LOG_INFO(Frontend, "Writing .ll");
	std::error_code error;
	raw_fd_ostream file(string(filename) + ".ll", error, sys::fs::OpenFlags::F_RW);

	if (file.has_error())
	{
		LOG_CRITICAL(Frontend, "Failed to write bitcode file: error %s", error.message().c_str());
	}

	module->print(file, nullptr);
	LOG_INFO(Frontend, "Done");
}

void Codegen::WriteFile(const char *filename)
{
	PassManager passManager;
	FunctionPassManager functionPassManager(module.get());
	PassManagerBuilder passManagerBuilder;

	//auto bb = blocks[0x1003ac]->basicBlock.get();
	//bb->dump();

	WriteLL(filename);
	raw_os_ostream os(std::cout);
	if (verifyModule(*module, &os))
	{
		os.flush();
		//WriteLL(filename);
		std::cin.get();
	}

	module->setDataLayout(nativeTarget->getSubtargetImpl()->getDataLayout());

	passManager.add(createVerifierPass());
	passManager.add(new TargetLibraryInfo(nativeTriple));
	passManager.add(new DataLayoutPass());
	nativeTarget->addAnalysisPasses(passManager);
	functionPassManager.add(new DataLayoutPass());
	nativeTarget->addAnalysisPasses(functionPassManager);

	passManagerBuilder.OptLevel = 3;
	passManagerBuilder.SizeLevel = 0;
	passManagerBuilder.Inliner = createFunctionInliningPass(3, 0);
	passManagerBuilder.LoopVectorize = true;
	passManagerBuilder.SLPVectorize = true;

	passManagerBuilder.populateFunctionPassManager(functionPassManager);
	passManagerBuilder.populateModulePassManager(passManager);

	LOG_INFO(Frontend, "Running function optimization");
	functionPassManager.doInitialization();
	for (auto &function : *module)
		functionPassManager.run(function);
	functionPassManager.doFinalization();

	passManager.add(createVerifierPass());

	MCContext *context;
	std::error_code error;
	raw_fd_ostream file(filename, error, sys::fs::OpenFlags::F_RW);
	if (file.has_error())
	{
		LOG_CRITICAL(Frontend, "Failed to write bitcode file: error %s", error.message().c_str());
		return;
	}
	if (nativeTarget->addPassesToEmitMC(passManager, context, file, false))
	{
		LOG_CRITICAL(Frontend, "Target does not support MC emission!");
		return;
	}
	LOG_INFO(Frontend, "Generating module");
	passManager.run(*module);
	file.flush();
	LOG_INFO(Frontend, "Done");
}

void Codegen::TranslateBlocks()
{
	size_t largestSize = 0, largestPc = 0;
	size_t translated = 0;
	CodeBlock *lastBlock = nullptr;
	size_t linked = 0, terminated = 0, jumpFailed = 0, disabled = 0;
	for (u32 i = TranslateStart; i < TranslateEnd; i += 4)
	{
		auto codeBlock = new CodeBlock(this, i);
		if (codeBlock->AddInstruction())
		{
			if (lastBlock && !lastBlock->jumpAddress)
			{
				lastBlock->jumpAddress = i;
			}
			blocks.insert(make_pair(i, codeBlock));
			++translated;
		}
		else
		{
			if (lastBlock)
			{
				if (lastBlock->lastBlock->getTerminator()) __debugbreak();
			}
			delete codeBlock;
			codeBlock = nullptr;
		}
		lastBlock = codeBlock;
	}

	for (auto p : blocks)
	{
		auto block = p.second;
		if (!block->jumpAddress) continue;
		if (block->disabled) continue;
		if (block->wrotePC) continue;
		auto i = blocks.find(block->jumpAddress);
		if (i != blocks.end())
		{
			CodeBlock::Link(block, i->second);
			++linked;
		}
	}
	for (auto p : blocks)
	{
		auto block = p.second;
		if (block->disabled)
		{
			block->jumpAddress = block->pc;
			block->JumpFailed();
			++disabled;
		}
		else
		{
			if (!block->jumpAddress) continue;
			auto i = blocks.find(block->jumpAddress);
			if (i == blocks.end())
			{
				block->JumpFailed();
				++jumpFailed;
			}
		}
	}
	for (auto block : blocks)
	{
		if (block.second->wrotePC)
		{
			block.second->TerminateWritePC();
		}
		else if (!block.second->jumpAddress)
		{
			block.second->Terminate();
			++terminated;
		}
	}

	cout << "Translated " << hex << translated << " of " << (TranslateSize / 4) << " = " << dec << (100.0 * translated / (TranslateSize / 4)) << "%" << endl;
	cout << "Total " << blocks.size() << " blocks" << endl;
	cout << linked <<     " linked     blocks = " << (100.0 * linked / blocks.size()) << "%" << endl;
	cout << terminated << " terminated blocks = " << (100.0 * terminated / blocks.size()) << "%" << endl;
	cout << jumpFailed << " jumpFailed blocks = " << (100.0 * jumpFailed / blocks.size()) << "%" << endl;
	cout << disabled << " disabled   blocks = " << (100.0 * disabled / blocks.size()) << "%" << endl;
}

void Codegen::CreateRegisters()
{
	/*registers = irBuilder->CreateAlloca(irBuilder->getInt32Ty(), irBuilder->getInt32(16));
	for (auto i = 0; i < 16; ++i)
	{
		auto src = irBuilder->CreateConstGEP1_32(registersArgument, i);
		auto load = irBuilder->CreateLoad(src);
		auto dst = irBuilder->CreateConstGEP1_32(registers, i);
		irBuilder->CreateStore(load, dst);
	}*/
}
void Codegen::StoreRegisters()
{
	/*for (auto i = 0; i < 16; ++i)
	{
		auto src = irBuilder->CreateConstGEP1_32(registers, i);
		auto load = irBuilder->CreateLoad(src);
		auto dst = irBuilder->CreateConstGEP1_32(registersArgument, i);
		irBuilder->CreateStore(load, dst);
	}*/
}

bool Codegen::CanRead(Register reg)
{
	return reg <= Register::SP || reg == Register::LR;
}
bool Codegen::CanWrite(Register reg)
{
	return reg <= Register::SP || reg == Register::LR;
}

llvm::Type *Codegen::RegType(Register reg)
{
	return reg < Register::N ? irBuilder->getInt32Ty() : irBuilder->getInt1Ty();
}
Value *Codegen::RegGEP(Register reg)
{
	auto src = reg < Register::N ? registersGlobal : flagsGlobal;
	auto offset = reg < Register::N ? (unsigned)reg : (((unsigned)reg - (unsigned)Register::N) * 4);
	auto load = irBuilder->CreateLoad(src);
	load->setMetadata(LLVMContext::MD_tbaa, mdRegistersGlobal);
	return irBuilder->CreateConstGEP1_32(load, offset);
}
Value *Codegen::Read(Register reg)
{
	auto load = irBuilder->CreateLoad(RegGEP(reg));
	load->setMetadata(LLVMContext::MD_tbaa, mdRegisters[(int)reg]);
	return load;
}
Value *Codegen::Write(Register reg, Value *val)
{
	auto store = irBuilder->CreateStore(val, RegGEP(reg));
	store->setMetadata(LLVMContext::MD_tbaa, mdRegisters[(int)reg]);
	return store;
}

llvm::Value* Codegen::Read32(llvm::Value* address)
{
	auto read32 = irBuilder->CreateLoad(read32Global);
	read32->setMetadata(LLVMContext::MD_tbaa, mdRead32);
	auto value = irBuilder->CreateCall(read32, address);
	value->setMetadata(LLVMContext::MD_tbaa, mdMemory);
	value->setOnlyReadsMemory();
#ifdef _WIN64
	value->setCallingConv(CallingConv::X86_64_Win64);
#endif
	return value;
}

void Codegen::Write32(llvm::Value* address, llvm::Value* value)
{
	auto write32 = irBuilder->CreateLoad(write32Global);
	write32->setMetadata(LLVMContext::MD_tbaa, mdRead32);
	auto call = irBuilder->CreateCall2(write32, address, value);
	call->setMetadata(LLVMContext::MD_tbaa, mdMemory);
#ifdef _WIN64
	call->setCallingConv(CallingConv::X86_64_Win64);
#endif
}

bool Codegen::CanCond(Condition cond)
{
	return cond != Condition::Invalid;
}