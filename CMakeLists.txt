cmake_minimum_required(VERSION 3.8)

project(xxhash_cpp  
	VERSION 0.8.1
	LANGUAGES CXX 
	DESCRIPTION "C++ port of the xxhash library." 
	HOMEPAGE_URL "https://github.com/RedSpah/xxhash_cpp")

include(GNUInstallDirs)

add_library(${PROJECT_NAME} INTERFACE)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

target_include_directories(${PROJECT_NAME} INTERFACE
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>  
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" AND APPLE)
	find_path(SSE2NEON_HEADER sse2neon.h PATHS ${CMAKE_SOURCE_DIR}/../sse2neon)
  target_include_directories(${PROJECT_NAME} INTERFACE ${SSE2NEON_HEADER})
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CTest)
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

install(TARGETS ${PROJECT_NAME} 
	EXPORT ${PROJECT_NAME}_Targets 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CMakePackageConfigHelpers)

write_basic_package_version_file("${PROJECT_NAME}ConfigVersion.cmake" 
	VERSION ${PROJECT_VERSION} 
	COMPATIBILITY ExactVersion)

file(WRITE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake.in
  "@PACKAGE_INIT@\n"
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/@PROJECT_NAME@Targets.cmake\")\n"
  "check_required_components(\"@PROJECT_NAME@\")\n"
)

configure_package_config_file(
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake.in" 
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake" 
	INSTALL_DESTINATION 
	${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(EXPORT ${PROJECT_NAME}_Targets 
	FILE ${PROJECT_NAME}Targets.cmake 
	NAMESPACE ${PROJECT_NAME}:: 
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake" 
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" 
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include DESTINATION include)
