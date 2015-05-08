#include <vector>

namespace llvm
{
	class BasicBlock;
	class Function;
	class FunctionType;
}
class InstructionBlock;
class ModuleGen;

/*

Responsible to partition the blocks by connectivity, each disjoined graph gets a color
And to generate a function for each color

*/

class BlockColors
{
public:
	BlockColors(ModuleGen *module);
	~BlockColors();

	void AddBlock(InstructionBlock *block);
	// Generates a function for each color
	void GenerateFunctions();

	llvm::FunctionType *GetFunctionType() { return function_type; }
	size_t GetColorCount() const { return colors.size(); }
    size_t GetColorInstructionCount(size_t color) const { return colors[color].instructions.size(); }
	InstructionBlock *GetColorInstruction(size_t color, size_t index) { return colors[color].instructions[index]; }
	llvm::Function *GetColorFunction(size_t color) { return colors[color].function; }
private:
	ModuleGen *module;

	// void ColorFunction(int i)
	// Runs the code for color->instructions[i]
	llvm::FunctionType *function_type;

	void AddBasicBlocksToFunction(llvm::Function *function, llvm::BasicBlock *basic_block);

	struct Color
	{
		size_t color;
		std::vector<InstructionBlock *> instructions;
		llvm::Function *function;
	};
	std::vector<Color> colors;
};