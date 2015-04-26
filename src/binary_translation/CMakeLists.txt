set(SRCS
			main.cpp
			CodeGen.cpp
            )
set(HEADERS
			CodeGen.h
            )
			
create_directory_groups(${SRCS} ${HEADERS})

add_executable(binary_translate ${SRCS} ${HEADERS})
target_link_libraries(binary_translate ${llvm_libs})
target_link_libraries(binary_translate core common video_core)
target_link_libraries(binary_translate ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih)
target_link_libraries(binary_translate ${PLATFORM_LIBRARIES})