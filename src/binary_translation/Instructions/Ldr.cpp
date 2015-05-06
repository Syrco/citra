#include "Ldr.h"
#include "Disassembler.h"
#include "InstructionBlock.h"
#include <llvm/IR/Value.h>
#include <core/loader/loader.h>
#include <core/mem_map.h>
#include "MachineState.h"

static RegisterInstruction<Ldr> register_instruction;

bool Ldr::Decode()
{
    if (ReadFields({ CondDef(), FieldDef<4>(5), FieldDef<1>(&U), FieldDef<7>(0x1f),
                     FieldDef<4>(&rt), FieldDef<12>(&imm12)}))
    {
        form = Form::PC;
        return true;
    }
    if (ReadFields({ CondDef(), FieldDef<3>(2), FieldDef<1>(&P), FieldDef<1>(&U), FieldDef<1>(0), FieldDef<1>(&W), FieldDef<1>(1), FieldDef<4>(&rn),
                     FieldDef<4>(&rt), FieldDef<12>(&imm12) }))
    {
        form = Form::Reg;

        if (!P && W) return false; // SEE LDRT;
        //if (rn == Register::SP && !P && U && !W && imm12 == 4) return false; // SEE POP;
        if ((!P || W) && rn == rt) return false; // UNPREDICTABLE;

        return true;
    }
    if (ReadFields({ CondDef(), FieldDef<6>(0x22), FieldDef<1>(&W), FieldDef<1>(1), FieldDef<4>(&rn),
                     FieldDef<16>(&register_list) }))
    {
        form = Form::MultiReg;

        //if (W && rn == Register::SP && register_list.size() > 1) return false; // SEE POP (ARM);
        if (rn == Register::PC || register_list.size() < 1) return false; // UNPREDICTABLE;
        if (W && register_list[(u32)rn]) return false; // UNPREDICTABLE;

        return true;
    }
    return false;
}

void Ldr::GenerateInstructionCode(InstructionBlock* instruction_block)
{
    auto ir_builder = instruction_block->IrBuilder();

    if (form != Form::MultiReg)
    {
        llvm::Value *address = nullptr;
        llvm::Value *value = nullptr;

        auto add = (bool)U;

        if (form == Form::PC)
        {
            auto base = instruction_block->Address() + 8;
            auto constAddress = add ? base + imm12 : base - imm12;
            auto constAddressEnd = constAddress + 4;
            // If the value is read only, inline it
            if (constAddress >= Loader::ROMCodeStart && constAddressEnd <= (Loader::ROMCodeStart + Loader::ROMCodeSize) ||
                constAddress >= Loader::ROMReadOnlyDataStart && constAddressEnd <= (Loader::ROMReadOnlyDataStart + Loader::ROMReadOnlyDataSize))
            {
                value = ir_builder->getInt32(Memory::Read32(constAddress));
            }
            else
            {
                address = ir_builder->getInt32(constAddress);
            }
        }
        else
        {
            auto index = P == 1;
            auto wback = P == 0 || W == 1;
            auto source_register = instruction_block->Read(rn);
            auto imm32 = ir_builder->getInt32(add ? imm12 : -imm12);

            auto offset_address =  ir_builder->CreateAdd(source_register, imm32);
            address = index ? offset_address : source_register;
            if (wback)
                instruction_block->Write(rn, offset_address);
        }

        if (!value) value = instruction_block->Module()->Machine()->ReadMemory32(address);
        instruction_block->Write(rt, value);

        if (rt == Register::PC)
            instruction_block->Module()->BranchReadPC();
    }
    else
    {
        auto wback = (bool)W;
        auto address = instruction_block->Read(rn);
        for (auto i = 0; i < 16; ++i)
        {
            if (!register_list[i]) continue;
            instruction_block->Write((Register)i, instruction_block->Module()->Machine()->ReadMemory32(address));
            address = ir_builder->CreateAdd(address, ir_builder->getInt32(4));
        }

        if (wback)
            instruction_block->Write(rn, address);

        if (register_list[15])
            instruction_block->Module()->BranchReadPC();
    }
}