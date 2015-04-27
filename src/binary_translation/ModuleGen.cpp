#include "ModuleGen.h"
#include "Disassembler.h"
#include "core/loader/loader.h"
#include "core/mem_map.h"
#include "Instructions/Instruction.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>

using namespace llvm;

ModuleGen::ModuleGen(llvm::Module* module)
    : module(module)
{
    ir_builder = make_unique<IRBuilder<>>(getGlobalContext());
}

ModuleGen::~ModuleGen()
{
}

void ModuleGen::Run()
{
    GenerateGlobals();
    GenerateCanRunFunction();
    GenerateRunFunction();

    GenerateBlocks();
}

void ModuleGen::GenerateGlobals()
{
    auto registers_global_initializer = ConstantPointerNull::get(IntegerType::getInt32PtrTy(getGlobalContext()));
    registers_global = new GlobalVariable(*module, registers_global_initializer->getType(), false, GlobalValue::ExternalLinkage, registers_global_initializer, "Registers");

    // Flags is stored internally as i1* indexed in multiples of 4
    auto flags_global_initializer = ConstantPointerNull::get(IntegerType::getInt1PtrTy(getGlobalContext()));
    flags_global = new GlobalVariable(*module, flags_global_initializer->getType(), false, GlobalValue::ExternalLinkage, flags_global_initializer, "Flags");
}

void ModuleGen::GenerateCanRunFunction()
{
    auto can_run_function_type = FunctionType::get(ir_builder->getInt1Ty(), false);
    can_run_function = Function::Create(can_run_function_type, GlobalValue::ExternalLinkage, "CanRun", module);
    auto basic_block = BasicBlock::Create(getGlobalContext(), "Entry", can_run_function);

    ir_builder->SetInsertPoint(basic_block);
    ir_builder->CreateRet(ir_builder->getInt1(false));
}

void ModuleGen::GenerateRunFunction()
{
    auto run_function_type = FunctionType::get(ir_builder->getVoidTy(), false);
    run_function = Function::Create(run_function_type, GlobalValue::ExternalLinkage, "Run", module);
    auto basic_block = BasicBlock::Create(getGlobalContext(), "Entry", run_function);

    ir_builder->SetInsertPoint(basic_block);
    ir_builder->CreateRetVoid();
}

void ModuleGen::GenerateBlocks()
{
    for (auto i = Loader::ROMCodeStart; i <= Loader::ROMCodeStart + Loader::ROMCodeSize - 4; i += 4)
    {
        auto instruction = Disassembler::Disassemble(Memory::Read32(i), i);
        if (instruction != nullptr)
        {
            LOG_DEBUG(BinaryTranslator, "Instruction at %08x", i);
        }
    }
}