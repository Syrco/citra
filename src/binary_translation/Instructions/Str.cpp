#include "Str.h"
#include "Disassembler.h"
#include "InstructionBlock.h"
#include <llvm/IR/Value.h>
#include <core/loader/loader.h>
#include <core/mem_map.h>
#include "MachineState.h"

static RegisterInstruction<Str> register_instruction;

bool Str::Decode()
{
    if (ReadFields({ CondDef(), FieldDef<3>(2), FieldDef<1>(&P), FieldDef<1>(&U), FieldDef<1>(0), FieldDef<1>(&W), FieldDef<1>(0), FieldDef<4>(&rn),
                     FieldDef<4>(&rt), FieldDef<12>(&imm12) }))
    {
        form = Form::Immediate;

        if (!P && W) return false; // SEE LDRT;
        if ((!P || W) && (rn == rt || rn == Register::PC)) return false; // UNPREDICTABLE;
        if (rn == Register::PC) return false; // Currently unimplemented

        return true;
    }
    if (ReadFields({ CondDef(), FieldDef<6>(0x24), FieldDef<1>(&W), FieldDef<1>(0), FieldDef<4>(&rn),
                     FieldDef<16>(&register_list) }))
    {
        form = Form::MultiReg;

        if (rn == Register::PC || register_list.size() < 1) return false; // UNPREDICTABLE;
        if (register_list[(int)Register::PC]) return false; // Currently unimplemented

        return true;
    }
    return false;
}

void Str::GenerateInstructionCode(InstructionBlock* instruction_block)
{
    auto ir_builder = instruction_block->IrBuilder();

    if (form == Form::Immediate)
    {
        auto add = U == 1;
        auto index = P == 1;
        auto wback = P == 0 || W == 1;
        auto source_register = instruction_block->Read(rn);
        auto imm32 = ir_builder->getInt32(add ? imm12 : -imm12);

        auto offset_address = ir_builder->CreateAdd(source_register, imm32);
        auto address = index ? offset_address : source_register;
        if (wback)
            instruction_block->Write(rn, offset_address);
        instruction_block->Module()->Machine()->WriteMemory32(address, instruction_block->Read(rt));
    }
    else
    {
        auto wback = W == 1;
        auto write_back_address = ir_builder->CreateSub(instruction_block->Read(rn), ir_builder->getInt32(4 * register_list.count()));
        auto address = write_back_address;
        for (auto i = 0; i < 16; ++i)
        {
            if (!register_list[i]) continue;
            instruction_block->Module()->Machine()->WriteMemory32(address, instruction_block->Read((Register)i));
            address = ir_builder->CreateAdd(address, ir_builder->getInt32(4));
        }

        if (wback)
            instruction_block->Write(rn, write_back_address);
    }
}