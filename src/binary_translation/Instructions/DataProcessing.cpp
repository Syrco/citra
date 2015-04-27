#include "DataProcessing.h"
#include "Disassembler.h"

static RegisterInstruction<DataProcessing> register_instruction;

bool DataProcessing::Decode()
{
    if (ReadFields({ FieldDef<4>(&cond), FieldDef<3>(1), FieldDef<4>(&short_op), FieldDef<1>(&s), FieldDef<4>(&rn),
                     FieldDef<4>(&rd), FieldDef<12>(&imm12) }))
    {
        form = Form::Immediate;
        return true;
    }
    return false;
}