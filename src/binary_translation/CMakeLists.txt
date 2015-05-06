set(SRCS
			main.cpp
			CodeGen.cpp
			ModuleGen.cpp
			Disassembler.cpp
			InstructionBlock.cpp
			MachineState.cpp
			TBAA.cpp
			ARMFuncs.cpp
            BlockColors.cpp
			
			Instructions/Instruction.cpp
			Instructions/MovShift.cpp
			Instructions/Branch.cpp
			Instructions/Arithmetic.cpp
			Instructions/Ldr.cpp
			Instructions/Str.cpp
            )
set(HEADERS
			CodeGen.h
			ModuleGen.h
			Disassembler.h
			InstructionBlock.h
			MachineState.h
			TBAA.h
			BinarySearch.h
			ARMFuncs.h
            BlockColors.h
			
			Instructions/Types.h
			Instructions/Instruction.h
			Instructions/MovShift.h
			Instructions/Branch.h
			Instructions/Arithmetic.h
			Instructions/Ldr.h
			Instructions/Str.h
            )
			
create_directory_groups(${SRCS} ${HEADERS})

include_directories(.)
add_executable(binary_translate ${SRCS} ${HEADERS})
target_link_libraries(binary_translate ${llvm_libs})
target_link_libraries(binary_translate core common video_core)
target_link_libraries(binary_translate ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih)
target_link_libraries(binary_translate ${PLATFORM_LIBRARIES})
