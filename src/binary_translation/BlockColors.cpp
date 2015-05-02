#include "BlockColors.h"
#include <stack>
#include "InstructionBlock.h"
#include <llvm/IR/Function.h>
#include "common/logging/log.h"

using namespace llvm;

BlockColors::BlockColors(ModuleGen* module) : module(module)
{
	auto ir_builder = module->IrBuilder();
	function_type = FunctionType::get(ir_builder->getVoidTy(), ir_builder->getInt32Ty(), false);
}

BlockColors::~BlockColors()
{
}

void BlockColors::AddBlock(InstructionBlock* block)
{
	if (block->HasColor()) return;

	std::stack<InstructionBlock *> current_color_stack;
	current_color_stack.push(block);
	auto color = colors.size();
	colors.push_back({ color });

	while (current_color_stack.size())
	{
		auto item = current_color_stack.top();
		current_color_stack.pop();

		item->SetColor(color);
		colors[color].instructions.push_back(item);
		for (auto next : item->GetNexts())
		{
			if (next->HasColor()) assert(next->GetColor() == color);
			else current_color_stack.push(next);
		}
		for (auto prev : item->GetPrevs())
		{
			if (prev->HasColor()) assert(prev->GetColor() == color);
			else current_color_stack.push(prev);
		}
	}
}

void BlockColors::GenerateFunctions()
{
	auto ir_builder = module->IrBuilder();

	LOG_INFO(BinaryTranslator, "%x block colors", colors.size());

	for (auto &color : colors)
	{
		auto function = Function::Create(function_type, GlobalValue::PrivateLinkage,
			"ColorFunction", module->Module());
		color.function = function;
		auto index = &function->getArgumentList().front();

		auto entry_basic_block = BasicBlock::Create(getGlobalContext(), "Entry", function);
		auto default_case_basic_block = BasicBlock::Create(getGlobalContext(), "Default", function);

		ir_builder->SetInsertPoint(default_case_basic_block);
		ir_builder->CreateUnreachable();

		ir_builder->SetInsertPoint(entry_basic_block);
		auto switch_instruction = ir_builder->CreateSwitch(index, default_case_basic_block, color.instructions.size());
		for (size_t i = 0; i < color.instructions.size(); ++i)
		{
			switch_instruction->addCase(ir_builder->getInt32(i), color.instructions[i]->GetEntryBasicBlock());
			AddBasicBlocksToFunction(function, color.instructions[i]->GetEntryBasicBlock());
		}
	}
}

void BlockColors::AddBasicBlocksToFunction(Function* function, BasicBlock* basic_block)
{
	if (basic_block->getParent())
	{
		assert(basic_block->getParent() == function);
		return;
	}

	basic_block->insertInto(function);
	auto terminator = basic_block->getTerminator();
	for (auto i = 0; i < terminator->getNumSuccessors(); ++i)
	{
		AddBasicBlocksToFunction(function, terminator->getSuccessor(i));
	}
}