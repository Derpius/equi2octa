cmake_minimum_required (VERSION 3.20)

project("equi2octa" CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP QUIET)

add_subdirectory("libs/glm")

add_executable(
	${PROJECT_NAME}
	"equi2octa.cpp"
)

target_include_directories(
	${PROJECT_NAME} PRIVATE
	"libs/glm"
	"libs/stb"
)

if (OpenMP_CXX_FOUND)
	target_link_libraries(
		${PROJECT_NAME} PRIVATE
		OpenMP::OpenMP_CXX
		libomp.lib
	)
endif()

target_link_libraries(
	${PROJECT_NAME} PRIVATE
	glm
)
