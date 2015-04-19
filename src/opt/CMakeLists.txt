set(SRCS
            main.cpp
			Codegen.cpp
			CodeBlock.cpp
            )
set(HEADERS
            Codegen.h
			CodeBlock.h
            )
			
create_directory_groups(${SRCS} ${HEADERS})

add_executable(copt ${SRCS} ${HEADERS})
target_link_libraries(copt ${llvm_libs})
target_link_libraries(copt core common video_core)
target_link_libraries(copt ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih)
target_link_libraries(copt ${PLATFORM_LIBRARIES})