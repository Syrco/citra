#include "Disassembler.h"
#include "Instruction.h"
#include <vector>

std::vector<RegisterInstructionBase::CreateFunctionType> g_read_functions;

RegisterInstructionBase::RegisterInstructionBase(CreateFunctionType create_function)
{
    g_read_functions.push_back(create_function);
}

std::unique_ptr<Instruction> Disassmbler::Disassemble(u32 instruction, u32 address)
{
    for (auto read_function : g_read_functions)
    {
        auto result = read_function(instruction, address);
        if (result != nullptr)
        {
            return std::unique_ptr<Instruction>(result);
        }
    }
    return nullptr;
}