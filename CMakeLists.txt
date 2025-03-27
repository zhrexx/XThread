cmake_minimum_required(VERSION 2.8.4)
project(XThread)
enable_testing()

find_package(Threads REQUIRED)

add_library(xthread STATIC src/xthread.c)
target_link_libraries(xthread ${CMAKE_THREAD_LIBS_INIT})
set_property(TARGET xthread APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_property(TARGET xthread APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_property(TARGET xthread PROPERTY POSITION_INDEPENDENT_CODE True)

option(XTHREAD_INSTALL "Install a static library for XThread")

if(XTHREAD_INSTALL)
  if(CMAKE_INSTALL_LIBDIR)
    install(TARGETS xthread ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  else()
    install(TARGETS xthread ARCHIVE DESTINATION lib)
  endif()

  if(CMAKE_INSTALL_INCLUDEDIR)
    install(FILES source/xthread.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  else()
    install(FILES source/xthread.h DESTINATION include)
  endif()
endif(TINYCTHREAD_INSTALL)
