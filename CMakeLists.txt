cmake_minimum_required(VERSION 3.12)
project(v4l2capturer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SAMPLE "Build sample" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBV4L2 REQUIRED libv4l2)

set(SRCS src/V4l2Capturer.c)
set(INCS include/V4l2Capturer.h)

add_library(${CMAKE_PROJECT_NAME} STATIC ${SRCS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${LIBV4L2_INCLUDE_DIRS})
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC include)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${INCS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBV4L2_LIBRARIES})

install(TARGETS ${CMAKE_PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})

if(${BUILD_SAMPLE})
    set(CMAKE_CXX_STANDARD 17)
    set(SAMPLE_SRCS sample/V4l2CapturerSample.cpp)
    add_executable(V4l2CapturerSample ${SAMPLE_SRCS})
    find_package(Threads REQUIRED)
    target_link_libraries(V4l2CapturerSample ${CMAKE_PROJECT_NAME} Threads::Threads)
    install(TARGETS V4l2CapturerSample
            RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
endif()
