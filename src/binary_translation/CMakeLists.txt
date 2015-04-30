set(SRCS
			main.cpp
			CodeGen.cpp
			ModuleGen.cpp
			Disassembler.cpp
			InstructionBlock.cpp
			MachineState.cpp
			TBAA.cpp
			
			Instructions/Instruction.cpp
			Instructions/DataProcessing.cpp
			Instructions/Branch.cpp
            )
set(HEADERS
			CodeGen.h
			ModuleGen.h
			Disassembler.h
			InstructionBlock.h
			MachineState.h
			TBAA.h
			BinarySearch.h
			
			Instructions/Types.h
			Instructions/Instruction.h
			Instructions/DataProcessing.h
			Instructions/Branch.h
            )
			
create_directory_groups(${SRCS} ${HEADERS})

include_directories(.)
add_executable(binary_translate ${SRCS} ${HEADERS})
target_link_libraries(binary_translate ${llvm_libs})
target_link_libraries(binary_translate core common video_core)
target_link_libraries(binary_translate ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih)
target_link_libraries(binary_translate ${PLATFORM_LIBRARIES})