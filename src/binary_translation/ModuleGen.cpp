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

using namespace llvm;

ModuleGen::ModuleGen(llvm::Module* module)
    : module(module)
{
    ir_builder = make_unique<IRBuilder<>>(getGlobalContext());
    machine = make_unique<MachineState>(this);
    tbaa = make_unique<TBAA>();
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
    AddInstructionsToRunFunction();

    GenerateBlockAddressArray();
}

void ModuleGen::ReadPC()
{
    ir_builder->CreateBr(run_function_re_entry);
}

void ModuleGen::WritePCConst(u32 pc)
{
    auto i = instruction_blocks_by_pc.find(pc);
    if (i != instruction_blocks_by_pc.end())
    {
        // Found instruction, jump to it
        ir_builder->CreateBr(i->second->GetEntryBasicBlock());
    }
    else
    {
        // Didn't find instruction, write PC and exit
        machine->WriteRegiser(Register::PC, ir_builder->getInt32(pc));
        ir_builder->CreateRetVoid();
    }
}

void ModuleGen::GenerateGlobals()
{
    machine->GenerateGlobals();

    auto get_block_address_function_type = FunctionType::get(ir_builder->getInt8PtrTy(), ir_builder->getInt32Ty(), false);
    get_block_address_function = Function::Create(get_block_address_function_type, GlobalValue::PrivateLinkage, "GetBlockAddress", module);

    auto can_run_function_type = FunctionType::get(ir_builder->getInt1Ty(), false);
    can_run_function = Function::Create(can_run_function_type, GlobalValue::ExternalLinkage, "CanRun", module);

    auto run_function_type = FunctionType::get(ir_builder->getVoidTy(), false);
    run_function = Function::Create(run_function_type, GlobalValue::ExternalLinkage, "Run", module);

    block_address_array_base = Loader::ROMCodeStart / 4;
    block_address_array_size = Loader::ROMCodeSize / 4;

    block_address_array_type = ArrayType::get(ir_builder->getInt8PtrTy(), block_address_array_size);
    block_address_array = new GlobalVariable(*module, block_address_array_type, true, GlobalValue::ExternalLinkage, nullptr, "BlockAddressArray");
}

void ModuleGen::GenerateBlockAddressArray()
{
    auto local_block_address_array_values = std::make_unique<Constant*[]>(block_address_array_size);
    
    std::fill(
        local_block_address_array_values.get(),
        local_block_address_array_values.get() + block_address_array_size,
        ConstantPointerNull::get(ir_builder->getInt8PtrTy()));

    for (auto i = 0; i < instruction_blocks.size(); ++i)
    {
        auto &block = instruction_blocks[i];
        auto entry_basic_block = block->GetEntryBasicBlock();
        auto index = block->Address() / 4 - block_address_array_base;
        local_block_address_array_values[index] = BlockAddress::get(entry_basic_block->getParent(), entry_basic_block);
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
        if(index < block_address_array_size)
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
    auto index = ir_builder->CreateUDiv(pc, ir_builder->getInt32(4));
    index = ir_builder->CreateSub(index, ir_builder->getInt32(block_address_array_base));
    auto in_bounds_pred = ir_builder->CreateICmpULT(index, ir_builder->getInt32(block_address_array_size));
    ir_builder->CreateCondBr(in_bounds_pred, index_in_bounds_basic_block, index_out_of_bounds_basic_block);

    ir_builder->SetInsertPoint(index_in_bounds_basic_block);
    Value *gep_values[] = { ir_builder->getInt32(0), index };
    auto block_address = ir_builder->CreateLoad(ir_builder->CreateInBoundsGEP(block_address_array, gep_values));
    tbaa->TagConst(block_address);
    ir_builder->CreateRet(block_address);

    ir_builder->SetInsertPoint(index_out_of_bounds_basic_block);
    ir_builder->CreateRet(ConstantPointerNull::get(ir_builder->getInt8PtrTy()));
}

void ModuleGen::GenerateCanRunFunction()
{
    // return GetBlockAddress(Read(PC)) != nullptr;
    auto basic_block = BasicBlock::Create(getGlobalContext(), "Entry", can_run_function);

    ir_builder->SetInsertPoint(basic_block);
    auto block_address = ir_builder->CreateCall(get_block_address_function, machine->ReadRegiser(Register::PC));
    ir_builder->CreateRet(ir_builder->CreateICmpNE(block_address, ConstantPointerNull::get(ir_builder->getInt8PtrTy())));
}

void ModuleGen::GenerateRunFunction()
{
    /*
    run_function_entry:
    run_function_re_entry:
        auto block_address = GetBlockAddress(Read(PC))
        if(index != nullptr)
        {
    block_present_basic_block:
            goto block_address;
            return;
        }
        else
        {
    block_not_present_basic_block:
            return;
        }
    */
    run_function_entry = BasicBlock::Create(getGlobalContext(), "Entry", run_function);
    // run_function_re_entry is needed because it isn't possible to jump to the first block of a function
    run_function_re_entry = BasicBlock::Create(getGlobalContext(), "ReEntry", run_function);
    auto block_present_basic_block = BasicBlock::Create(getGlobalContext(), "BlockPresent", run_function);
    auto block_not_present_basic_block = BasicBlock::Create(getGlobalContext(), "BlockNotPresent", run_function);

    ir_builder->SetInsertPoint(run_function_entry);
    ir_builder->CreateBr(run_function_re_entry);

    ir_builder->SetInsertPoint(run_function_re_entry);
    auto block_address = ir_builder->CreateCall(get_block_address_function, Machine()->ReadRegiser(Register::PC));
    auto block_present_pred = ir_builder->CreateICmpNE(block_address, ConstantPointerNull::get(ir_builder->getInt8PtrTy()));
    ir_builder->CreateCondBr(block_present_pred, block_present_basic_block, block_not_present_basic_block);

    ir_builder->SetInsertPoint(block_present_basic_block);
    auto indirect_br = ir_builder->CreateIndirectBr(block_address, instruction_blocks.size());
    for (auto &block : instruction_blocks)
    {
        indirect_br->addDestination(block->GetEntryBasicBlock());
    }

    ir_builder->SetInsertPoint(block_not_present_basic_block);
    ir_builder->CreateRetVoid();
}

void ModuleGen::DecodeInstructions()
{
    for (auto i = Loader::ROMCodeStart; i <= Loader::ROMCodeStart + Loader::ROMCodeSize - 4; i += 4)
    {
        auto instruction = Disassembler::Disassemble(Memory::Read32(i), i);
        if (instruction == nullptr) continue;
        auto instruction_block = std::make_unique<InstructionBlock>(this, instruction.release());
        instruction_blocks_by_pc[i] = instruction_block.get();
        instruction_blocks.push_back(std::move(instruction_block));
    }
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

void ModuleGen::AddInstructionsToRunFunction()
{
    std::stack<BasicBlock *> basic_blocks_stack;

    for (auto &block : instruction_blocks)
    {
        basic_blocks_stack.push(block->GetEntryBasicBlock());

        while (basic_blocks_stack.size())
        {
            auto basic_block = basic_blocks_stack.top();
            basic_blocks_stack.pop();
            if (basic_block->getParent()) continue; // Already added to run
            basic_block->insertInto(run_function);
            auto terminator = basic_block->getTerminator();
            for (auto i = 0; i < terminator->getNumSuccessors(); ++i)
            {
                auto new_basic_block = terminator->getSuccessor(i);
                if (new_basic_block->getParent()) continue; // Already added to run
                basic_blocks_stack.push(new_basic_block);
            }
        }
    }
}