#include "MachineState.h"
#include "ModuleGen.h"
#include "Instructions/Types.h"
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/GlobalVariable.h>
#include "TBAA.h"

using namespace llvm;

MachineState::MachineState(ModuleGen *module) : module(module)
{
}

void MachineState::GenerateGlobals()
{
    auto ir_builder = module->IrBuilder();

    auto registers_global_initializer = ConstantPointerNull::get(IntegerType::getInt32PtrTy(getGlobalContext()));
    registers_global = new GlobalVariable(*module->Module(), registers_global_initializer->getType(),
        false, GlobalValue::ExternalLinkage, registers_global_initializer, "Registers");

    // Flags is stored internally as i1* indexed in multiples of 4
    auto flags_global_initializer = ConstantPointerNull::get(IntegerType::getInt1PtrTy(getGlobalContext()));
    flags_global = new GlobalVariable(*module->Module(), flags_global_initializer->getType(),
        false, GlobalValue::ExternalLinkage, flags_global_initializer, "Flags");

    auto memory_read_32_signature = FunctionType::get(ir_builder->getInt32Ty(), ir_builder->getInt32Ty(), false);
    auto memory_read_32_ptr = PointerType::get(memory_read_32_signature, 0);
    auto memory_read_32_initializer = ConstantPointerNull::get(memory_read_32_ptr);
    memory_read_32_global = new GlobalVariable(*module->Module(), memory_read_32_ptr,
        false, GlobalValue::ExternalLinkage, memory_read_32_initializer, "Memory::Read32");

    llvm::Type *memory_write_32_args[] = { ir_builder->getInt32Ty(), ir_builder->getInt32Ty() };
    auto memory_write_32_signature = FunctionType::get(ir_builder->getVoidTy(), memory_write_32_args, false);
    auto memory_write_32_ptr = PointerType::get(memory_write_32_signature, 0);
    auto memory_write_32_initializer = ConstantPointerNull::get(memory_write_32_ptr);
    memory_write_32_global = new GlobalVariable(*module->Module(), memory_write_32_ptr,
        false, GlobalValue::ExternalLinkage, memory_write_32_initializer, "Memory::Write32");
}

Value *MachineState::GetRegisterPtr(Register reg)
{
    Value *global;
    unsigned index;
    if (reg <= Register::PC)
    {
        global = registers_global;
        index = static_cast<unsigned>(reg)-static_cast<unsigned>(Register::R0);
    }
    else
    {
        global = flags_global;
        index = (static_cast<unsigned>(reg)-static_cast<unsigned>(Register::N)) * 4;
    }
    auto base = module->IrBuilder()->CreateAlignedLoad(global, 4);
    module->GetTBAA()->TagConst(base);
    return module->IrBuilder()->CreateConstInBoundsGEP1_32(base, index);
}

Value* MachineState::ReadRegiser(Register reg, bool allow_pc)
{
    assert(allow_pc || reg != Register::PC);
    auto load = module->IrBuilder()->CreateAlignedLoad(GetRegisterPtr(reg), 4);
    module->GetTBAA()->TagRegister(load, reg);
    return load;
}

Value* MachineState::WriteRegiser(Register reg, Value *value)
{
    auto store = module->IrBuilder()->CreateAlignedStore(value, GetRegisterPtr(reg), 4);
    module->GetTBAA()->TagRegister(store, reg);
    return store;
}

Value* MachineState::ConditionPassed(Condition cond)
{
    auto ir_builder = module->IrBuilder();
    Value *pred = nullptr;
    auto not = false;
    switch (cond)
    {
    case Condition::NE: case Condition::CC: case Condition::PL: case Condition::VC:
    case Condition::LS: case Condition::LT: case Condition::LE:
        not = true;
        cond = (Condition)((int)cond - 1);
    }

    switch (cond)
    {
    case Condition::EQ: pred = ReadRegiser(Register::Z); break;
    case Condition::CS: pred = ReadRegiser(Register::C); break;
    case Condition::MI: pred = ReadRegiser(Register::N); break;
    case Condition::VS: pred = ReadRegiser(Register::V); break;
    case Condition::HI: pred = ir_builder->CreateAnd(ReadRegiser(Register::C), ir_builder->CreateNot(ReadRegiser(Register::Z))); break;
    case Condition::GE: pred = ir_builder->CreateICmpEQ(ReadRegiser(Register::N), ReadRegiser(Register::V)); break;
    case Condition::GT: pred = ir_builder->CreateAnd(ir_builder->CreateNot(ReadRegiser(Register::Z)),
        ir_builder->CreateICmpEQ(ReadRegiser(Register::N), ReadRegiser(Register::V))); break;
    case Condition::AL: pred = ir_builder->getInt1(true);
    default: assert(false, "Invalid condition");
    }

    if (not) pred = ir_builder->CreateNot(pred);
    return pred;
}

llvm::Value* MachineState::ReadMemory32(llvm::Value* address)
{
    auto ir_builder = module->IrBuilder();

    auto memory_read_32 = ir_builder->CreateLoad(memory_read_32_global);
    module->GetTBAA()->TagConst(memory_read_32);

    auto call = ir_builder->CreateCall(memory_read_32, address);
    call->setOnlyReadsMemory();
    module->GetTBAA()->TagMemory(call);
    return call;
}

llvm::Value* MachineState::WriteMemory32(llvm::Value* address, llvm::Value* value)
{
    auto ir_builder = module->IrBuilder();

    auto memory_write_32 = ir_builder->CreateLoad(memory_write_32_global);
    module->GetTBAA()->TagConst(memory_write_32);

    auto call = ir_builder->CreateCall2(memory_write_32, address, value);
    module->GetTBAA()->TagMemory(call);
    return value;
}