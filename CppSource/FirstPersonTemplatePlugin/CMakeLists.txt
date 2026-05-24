#pl-version 2

pl_cmake_init()

# Get the name of this folder as the project name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

pl_create_target(SHARED_LIBRARY ${PROJECT_NAME} NO_PL_PREFIX NO_UNITY)

# include the PL natvis file for a better debugging experience
get_property(PL_ROOT_DIR GLOBAL PROPERTY PL_ROOT)
target_sources(${PROJECT_NAME} PUBLIC "${PL_ROOT_DIR}/Code/Engine/Foundation/plEngine.natvis")

target_link_libraries(${PROJECT_NAME}
  PUBLIC
  GameEngine
  Utilities
)

# Include optional extensions set up by the Editor
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Configs/CMakeEngineExtensions.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../Configs/CMakeEngineExtensions.txt")
endif()

# Include optional user extensions for custom target configuration
# This file can be used to add additional link libraries, includes, etc. without modifying this template
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CMakeExtensions.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/CMakeExtensions.txt")
endif()