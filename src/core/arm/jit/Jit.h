#include <memory>
#include <common/common_types.h>

class Jit
{
public:
	struct Private;

	Jit();

	void BeforeFindBB(struct ARMul_State* cpu);
	bool CanRun(u32 pc);
	void Run(struct ARMul_State* cpu);

private:
	std::unique_ptr<Private> priv;
};