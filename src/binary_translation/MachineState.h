
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
    llvm::Value *ReadRegiser(Register reg);
    llvm::Value *WriteRegiser(Register reg, llvm::Value *value);
    llvm::Value* ConditionPassed(Condition cond);
    llvm::Value* ReadMemory32(llvm::Value* address);
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
    * u32 (u32) Memory::Read
    *  Reads the memory at address
    */
    llvm::GlobalVariable *memory_read_32_global;
};