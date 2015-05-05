#include "ModuleGen.h"
#include "Disassembler.h"
#include "core/loader/loader.h"
#include "core/mem_map.h"
#include "Instructions/Instruction.h"
#include "Instructions/Types.h"
#include "InstructionBlock.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <stack>
#include "MachineState.h"
#include "TBAA.h"
#include "BlockColors.h"

using namespace llvm;

ModuleGen::ModuleGen(llvm::Module* module, bool verify)
    : module(module),
      verify(verify)
{
    ir_builder = make_unique<IRBuilder<>>(getGlobalContext());
    machine = make_unique<MachineState>(this);
    tbaa = make_unique<TBAA>();
	block_colors = make_unique<BlockColors>(this);
}

ModuleGen::~ModuleGen()
{
}

void ModuleGen::Run()
{
    tbaa->GenerateTags();
    GenerateGlobals();

    DecodeInstructions();
    GenerateInstructionsEntry();

    GenerateCanRunFunction();
    GenerateRunFunction();
    GenerateGetBlockAddressFunction();

    GenerateInstructionsCode();

	ColorBlocks();
    GenerateBlockAddressArray();
}

void ModuleGen::BranchReadPC()
{
    if (verify)
    {
        ir_builder->CreateRetVoid();
    }
    else
    {
        auto call = ir_builder->CreateCall(run_function);
        call->setTailCall();
        ir_builder->CreateRetVoid();
    }
}

void ModuleGen::BranchWritePCConst(InstructionBlock *current, u32 pc)
{
    if (verify)
    {
        // Just write PC and exit on verify
        machine->WriteRegiser(Register::PC, ir_builder->getInt32(pc));
        ir_builder->CreateRetVoid();
    }
    else
    {
        auto i = instruction_blocks_by_pc.find(pc);
        if (i != instruction_blocks_by_pc.end())
        {
            // Found instruction, jump to it
            ir_builder->CreateBr(i->second->GetEntryBasicBlock());
            InstructionBlock::Link(i->second, current);
        }
        else
        {
            // Didn't find instruction, write PC and exit
            machine->WriteRegiser(Register::PC, ir_builder->getInt32(pc));
            ir_builder->CreateRetVoid();
        }
    }
}

void ModuleGen::GenerateGlobals()
{
    machine->GenerateGlobals();

	auto function_pointer = PointerType::get(block_colors->GetFunctionType(), 0);
	block_address_type = StructType::get(function_pointer, ir_builder->getInt32Ty(), nullptr);
	block_address_not_present = ConstantStruct::get(block_address_type, ConstantPointerNull::get(function_pointer), ir_builder->getInt32(0), nullptr);
	
	auto get_block_address_function_type = FunctionType::get(block_address_type, ir_builder->getInt32Ty(), false);
    get_block_address_function = Function::Create(get_block_address_function_type, GlobalValue::PrivateLinkage, "GetBlockAddress", module);

    auto can_run_function_type = FunctionType::get(ir_builder->getInt1Ty(), false);
    can_run_function = Function::Create(can_run_function_type, GlobalValue::ExternalLinkage, "CanRun", module);

    auto run_function_type = FunctionType::get(ir_builder->getVoidTy(), false);
    run_function = Function::Create(run_function_type, GlobalValue::ExternalLinkage, "Run", module);

    block_address_array_base = Loader::ROMCodeStart / 4;
    block_address_array_size = Loader::ROMCodeSize / 4;

	block_address_array_type = ArrayType::get(block_address_type, block_address_array_size);
    block_address_array = new GlobalVariable(*module, block_address_array_type, true, GlobalValue::ExternalLinkage, nullptr, "BlockAddressArray");

    // bool Verify - contains the value of verify for citra usage
    new GlobalVariable(*module, ir_builder->getInt1Ty(), true, GlobalValue::ExternalLinkage, ir_builder->getInt1(verify), "Verify");
}

void ModuleGen::GenerateBlockAddressArray()
{
    auto local_block_address_array_values = std::make_unique<Constant*[]>(block_address_array_size);
    
    std::fill(
        local_block_address_array_values.get(),
        local_block_address_array_values.get() + block_address_array_size,
		block_address_not_present);

    /*for (auto i = 0; i < instruction_blocks.size(); ++i)
    {
        auto &block = instruction_blocks[i];
        auto entry_basic_block = block->GetEntryBasicBlock();
        auto index = block->Address() / 4 - block_address_array_base;
		auto color_index = 0;
        local_block_address_array_values[index] = BConst
    }*/
	for (auto color = 0; color < block_colors->GetColorCount(); ++color)
	{
		auto function = block_colors->GetColorFunction(color);
		for (auto i = 0; i < block_colors->GetColorInstructionCount(color); ++i)
		{
			auto block = block_colors->GetColorInstruction(color, i);
			auto index = block->Address() / 4 - block_address_array_base;
			auto value = ConstantStruct::get(block_address_type, function, ir_builder->getInt32(i), nullptr);
			local_block_address_array_values[index] = value;
		}
	}

    auto local_block_address_array_values_ref = ArrayRef<Constant*>(local_block_address_array_values.get(), block_address_array_size);
    auto local_blocks_address_array = ConstantArray::get(block_address_array_type, local_block_address_array_values_ref);
    block_address_array->setInitializer(local_blocks_address_array);
}

void ModuleGen::GenerateGetBlockAddressFunction()
{
    /*
    entry_basic_block:
        auto index = (pc - block_address_array_base) / 4;
        if(((pc & 3) == 0) && index < block_address_array_size)
        {
    index_in_bounds_basic_block:
            return block_address_array[index];
        }
        else
        {
    index_out_of_bounds_basic_block:
            return nullptr;
        }
    */
    auto pc = &*get_block_address_function->arg_begin();
    auto entry_basic_block = BasicBlock::Create(getGlobalContext(), "Entry", get_block_address_function);
    auto index_in_bounds_basic_block = BasicBlock::Create(getGlobalContext(), "IndexInBounds", get_block_address_function);
    auto index_out_of_bounds_basic_block = BasicBlock::Create(getGlobalContext(), "IndexOutOfBounds", get_block_address_function);

    ir_builder->SetInsertPoint(entry_basic_block);
    auto index = ir_builder->CreateUDiv(pc, ir_builder->getInt32(4), "", true);
    index = ir_builder->CreateSub(index, ir_builder->getInt32(block_address_array_base));
    auto in_bounds_pred = ir_builder->CreateICmpULT(index, ir_builder->getInt32(block_address_array_size));
    auto arm_pred = ir_builder->CreateICmpEQ(ir_builder->CreateAnd(pc, 3), ir_builder->getInt32(0));
    auto pred = ir_builder->CreateAnd(in_bounds_pred, arm_pred);
    ir_builder->CreateCondBr(pred, index_in_bounds_basic_block, index_out_of_bounds_basic_block);

    ir_builder->SetInsertPoint(index_in_bounds_basic_block);
    Value *gep_values[] = { ir_builder->getInt32(0), index };
    auto block_address = ir_builder->CreateLoad(ir_builder->CreateInBoundsGEP(block_address_array, gep_values));
    tbaa->TagConst(block_address);
    ir_builder->CreateRet(block_address);

    ir_builder->SetInsertPoint(index_out_of_bounds_basic_block);
    ir_builder->CreateRet(block_address_not_present);
}

void ModuleGen::GenerateCanRunFunction()
{
    // return GetBlockAddress(Read(PC)).function != nullptr;
    auto basic_block = BasicBlock::Create(getGlobalContext(), "Entry", can_run_function);

    ir_builder->SetInsertPoint(basic_block);
    auto block_address = ir_builder->CreateCall(get_block_address_function, machine->ReadRegiser(Register::PC));
	auto function = ir_builder->CreateExtractValue(block_address, 0);
	ir_builder->CreateRet(ir_builder->CreateICmpNE(function,
		ConstantPointerNull::get(cast<PointerType>(function->getType()))));
}

void ModuleGen::GenerateRunFunction()
{
    /*
    run_function_entry:
        auto block = GetBlockAddress(Read(PC))
        if(block_address != nullptr)
        {
    block_present_basic_block:
            block.function(block.index);
            return;
        }
        else
        {
    block_not_present_basic_block:
            return;
        }
    */
    run_function_entry = BasicBlock::Create(getGlobalContext(), "Entry", run_function);
    auto block_present_basic_block = BasicBlock::Create(getGlobalContext(), "BlockPresent", run_function);
    auto block_not_present_basic_block = BasicBlock::Create(getGlobalContext(), "BlockNotPresent", run_function);

	ir_builder->SetInsertPoint(run_function_entry);
	auto block_address = ir_builder->CreateCall(get_block_address_function, Machine()->ReadRegiser(Register::PC));
	auto function = ir_builder->CreateExtractValue(block_address, 0);
	auto block_present_pred = ir_builder->CreateICmpNE(function,
		ConstantPointerNull::get(cast<PointerType>(function->getType())));
    ir_builder->CreateCondBr(block_present_pred, block_present_basic_block, block_not_present_basic_block);

    ir_builder->SetInsertPoint(block_present_basic_block);
	auto index = ir_builder->CreateExtractValue(block_address, 1);
	auto call = ir_builder->CreateCall(function, index);
	call->setTailCall();
	ir_builder->CreateRetVoid();

    ir_builder->SetInsertPoint(block_not_present_basic_block);
    ir_builder->CreateRetVoid();
}

void ModuleGen::DecodeInstructions()
{
    size_t generated = 0;
    size_t total = 0;
    for (auto i = Loader::ROMCodeStart; i <= Loader::ROMCodeStart + Loader::ROMCodeSize - 4; i += 4)
    {
        ++total;
        auto instruction = Disassembler::Disassemble(Memory::Read32(i), i);
        if (instruction == nullptr) continue;
        ++generated;
        auto instruction_block = std::make_unique<InstructionBlock>(this, instruction.release());
        instruction_blocks_by_pc[i] = instruction_block.get();
        instruction_blocks.push_back(std::move(instruction_block));
    }

    LOG_INFO(BinaryTranslator, "Generated % 8d blocks of % 8d = % 3.1f%%", generated, total, 100.0 * generated / total);
}

void ModuleGen::GenerateInstructionsEntry()
{
    for (auto &instruction : instruction_blocks)
    {
        instruction->GenerateEntryBlock();
    }
}

void ModuleGen::GenerateInstructionsCode()
{
    for (auto &instruction : instruction_blocks)
    {
        instruction->GenerateCode();
    }
}

void ModuleGen::ColorBlocks()
{
	for (auto &instruction : instruction_blocks)
	{
		block_colors->AddBlock(instruction.get());
	}
	block_colors->GenerateFunctions();
}