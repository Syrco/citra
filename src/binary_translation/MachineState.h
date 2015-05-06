
enum class Condition;
enum class Register;
class ModuleGen;

namespace llvm
{
    class Value;
    class Instruction;
    class GlobalVariable;
}

/*
Contains all the machine state:
    Registers, Flags, Memory
*/
class MachineState
{
public:
    MachineState(ModuleGen *module);

    void GenerateGlobals();
    // allow_pc exists because most of the times reading the PC is not what the instruction meant
    llvm::Value *ReadRegiser(Register reg, bool allow_pc = false);
    llvm::Value *WriteRegiser(Register reg, llvm::Value *value);
    llvm::Value* ConditionPassed(Condition cond);
    llvm::Value* ReadMemory32(llvm::Value* address);
    llvm::Value* WriteMemory32(llvm::Value* address, llvm::Value* value);
private:
    // Returns the address of a register or a flag
    llvm::Value *GetRegisterPtr(Register reg);

    ModuleGen *module;

    /*
    * u32 *Registers;
    *  The registers of the cpu
    */
    llvm::GlobalVariable *registers_global;
    /*
    * u32 *Flags;
    *  The flags of the cpu
    *  Orderered N, Z, C, V
    */
    llvm::GlobalVariable *flags_global;

    /*
    * u32 (u32) Memory::Read32
    *  Reads the memory at address
    */
    llvm::GlobalVariable *memory_read_32_global;

    /*
    * void (u32, u32) Memory::Write32
    *  Writes the memory at address
    */
    llvm::GlobalVariable *memory_write_32_global;
};