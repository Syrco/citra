set(SRCS
            main.cpp
			Codegen.cpp
			CodeBlock.cpp
			Instruction.cpp
			Decode.cpp
			
			Instructions/Mov.cpp
			Instructions/Branch.cpp
			Instructions/Cmp.cpp
			Instructions/Ldr.cpp
			Instructions/Arith.cpp
            )
set(HEADERS
            Codegen.h
			CodeBlock.h
			Instruction.h
			Instructions.h
			Decode.h
			
			Instructions/Mov.h
			Instructions/Branch.h
			Instructions/Cmp.h
			Instructions/Ldr.h
			Instructions/Arith.h
            )
			
create_directory_groups(${SRCS} ${HEADERS})

include_directories(.)
add_executable(copt ${SRCS} ${HEADERS})
target_link_libraries(copt ${llvm_libs})
target_link_libraries(copt core common video_core)
target_link_libraries(copt ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih)
target_link_libraries(copt ${PLATFORM_LIBRARIES})