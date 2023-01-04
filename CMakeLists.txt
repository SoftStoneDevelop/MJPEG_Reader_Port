include(./.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
message(STATUS "Local .env.cmake: ${LOCAL_ENV}")

cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NAME MJPEGReader)
project(${NAME} VERSION 0.1.0)

# Add source to this project's executable.
file(GLOB_RECURSE SOURCES 
${PROJECT_SOURCE_DIR}/Src/*.cpp
${PROJECT_SOURCE_DIR}/External/imgui/*.cpp
)
add_executable(${PROJECT_NAME} ${SOURCES})

# If IMGUI_PATH not specified in .env.cmake, try fetching from git repo
if (NOT IMGUI_PATH)
  message(STATUS "IMGUI_PATH not specified in .env.cmake, using external/imgui")
  set(IMGUI_PATH External/imgui)
endif()

# If STB_IMAGE_PATH not specified in .env.cmake, try fetching from git repo
if (NOT STB_IMAGE_PATH)
  message(STATUS "STB_IMAGE_PATH not specified in .env.cmake, using external/stb")
  set(STB_IMAGE_PATH External/stb)
endif()

set(ArrayPool_PATH ArrayPool/Src/)
set(ThreadPool_PATH ThreadPool/Src/)

if (DEFINED VULKAN_SDK_PATH)
  set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include") # 1.1 Make sure this include path is correct
  message(STATUS "Found Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
  set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib") # 1.2 Make sure lib path is correct
  message(STATUS "Found Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
  set(Vulkan_FOUND "True")
else()
  find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
  message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
endif()
if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
else()
	message(STATUS "Using vulkan lib at: ${Vulkan_LIBRARIES}")
endif()

if (DEFINED GLFW_PATH)
  message(STATUS "Using GLFW path specified in .env")
  set(GLFW_INCLUDE_DIRS "${GLFW_PATH}/include")
  if (MSVC)
    set(GLFW_LIB "${GLFW_PATH}/lib-vc2022") # 2.1 Update lib-vc2022 to use same version as your visual studio
  elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    message(STATUS "USING MINGW")
    set(GLFW_LIB "${GLFW_PATH}/lib-mingw-w64") # 2.1 make sure matches glfw mingw subdirectory
  endif()
else()
  find_package(glfw3 3.3 REQUIRED)
  set(GLFW_LIB glfw)
  message(STATUS "Found GLFW")
endif()
if (NOT GLFW_LIB)
	message(FATAL_ERROR "Could not find glfw library!")
else()
	message(STATUS "Using glfw lib at: ${GLFW_LIB}")
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

if (WIN32)
  message(STATUS "CREATING BUILD FOR WINDOWS")

  if (USE_MINGW)
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/include
    )
    target_link_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/lib
    )
  endif()

  target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${ArrayPool_PATH}
    ${ThreadPool_PATH}
    ${IMGUI_PATH}
    ${STB_IMAGE_PATH}
    )

  target_link_directories(${PROJECT_NAME} PUBLIC
    ${Vulkan_LIBRARIES}
    ${GLFW_LIB}
  )

  target_link_libraries(${PROJECT_NAME} glfw3)
  target_link_libraries(${PROJECT_NAME} vulkan-1 -vulkan)
elseif (UNIX)
    message(STATUS "CREATING BUILD FOR UNIX")
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${PROJECT_SOURCE_DIR}/src
      ${ArrayPool_PATH}
      ${ThreadPool_PATH}
      ${IMGUI_PATH}
      ${STB_IMAGE_PATH}
    )
    target_link_libraries(${PROJECT_NAME} glfw ${Vulkan_LIBRARIES})
endif()