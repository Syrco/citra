#include <common/common_types.h>

/*
 * Functions from the manual,
 * A8.4.3 Pseudocode details of instruction-specified shifts and rotates
 * A2.2.1 Integer arithmetic 
 */

class InstructionBlock;

namespace llvm
{
    class Value;
}

class ARMFuncs
{
public:
    enum class SRType { LSL, LSR, ASR, RRX, ROR };
    struct ShiftTN
    {
        SRType type;
        llvm::Value *amount;
    };
    struct ResultCarry
    {
        llvm::Value *result, *carry;
    };

    static ShiftTN DecodeImmShift(InstructionBlock *instruction, u32 type, u32 imm5);
    static SRType DecodeRegShift(u32 type);

    static llvm::Value *Shift(InstructionBlock *instruction, llvm::Value *value, SRType type, llvm::Value *amount, llvm::Value *carry_in);
    static ResultCarry Shift_C(InstructionBlock *instruction, llvm::Value *value, SRType type, llvm::Value *amount, llvm::Value *carry_in);
    static ResultCarry LSL_C(InstructionBlock *instruction, llvm::Value *x, llvm::Value *shift);
    static llvm::Value *LSL(InstructionBlock *instruction, llvm::Value *x, llvm::Value *shift);
    static ResultCarry LSR_C(InstructionBlock *instruction, llvm::Value *x, llvm::Value *shift);
    static llvm::Value *LSR(InstructionBlock *instruction, llvm::Value *x, llvm::Value *shift);
    static ResultCarry ASR_C(InstructionBlock *instruction, llvm::Value *x, llvm::Value *shift);
    static ResultCarry ROR_C(InstructionBlock *instruction, llvm::Value *x, llvm::Value *shift);
    static ResultCarry RRX_C(InstructionBlock *instruction, llvm::Value *x, llvm::Value *carry_in);
};