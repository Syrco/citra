#include "MachineState.h"
#include "ModuleGen.h"
#include "Instructions/Types.h"
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/GlobalVariable.h>

using namespace llvm;

MachineState::MachineState(ModuleGen *module) : module(module)
{
}

void MachineState::GenerateGlobals()
{
    auto registers_global_initializer = ConstantPointerNull::get(IntegerType::getInt32PtrTy(getGlobalContext()));
    registers_global = new GlobalVariable(*module->Module(), registers_global_initializer->getType(),
        false, GlobalValue::ExternalLinkage, registers_global_initializer, "Registers");

    // Flags is stored internally as i1* indexed in multiples of 4
    auto flags_global_initializer = ConstantPointerNull::get(IntegerType::getInt1PtrTy(getGlobalContext()));
    flags_global = new GlobalVariable(*module->Module(), flags_global_initializer->getType(),
        false, GlobalValue::ExternalLinkage, flags_global_initializer, "Flags");
}

Value* MachineState::ReadRegiser(Register reg)
{
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
        index = static_cast<unsigned>(reg)-static_cast<unsigned>(Register::N);
    }
    auto base = module->IrBuilder()->CreateAlignedLoad(global, 4);
    module->GetTBAA()->TagConst(base);
    return module->IrBuilder()->CreateConstInBoundsGEP1_32(base, index);
}