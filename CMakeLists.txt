cmake_minimum_required(VERSION 4.1)
project(DEAGUI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32 AND NOT Qt6_DIR)
    if(DEFINED ENV{QTDIR} AND EXISTS "$ENV{QTDIR}/lib/cmake/Qt6")
        set(Qt6_DIR "$ENV{QTDIR}/lib/cmake/Qt6" CACHE PATH "Qt6 CMake package" FORCE)
    elseif(EXISTS "C:/Qt/6.10.2/mingw_64/lib/cmake/Qt6")
        set(Qt6_DIR "C:/Qt/6.10.2/mingw_64/lib/cmake/Qt6" CACHE PATH "Qt6 CMake package" FORCE)
    endif()
endif()

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Charts
        REQUIRED)



# AGL.framework exists on macOS 26+ as an empty stub (no binary), causing link failures.
# Qt's FindWrapOpenGL.cmake injects it via find_library(); strip it via regex filter.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(_wrap_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(_wrap_libs)
        list(FILTER _wrap_libs EXCLUDE REGEX "AGL")
        set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_wrap_libs}")
    endif()
endif()

add_executable(DEAGUI main.cpp mainwindow.cpp mainwindow.h
        resilience_indicators.h)
target_link_libraries(DEAGUI
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Charts
)

if(WIN32)
    add_custom_command(TARGET DEAGUI POST_BUILD
            COMMAND ${Qt6_DIR}/../../../bin/windeployqt.exe --dir ${CMAKE_CURRENT_BINARY_DIR} $<TARGET_FILE:DEAGUI>
            COMMENT "Running windeployqt..."
    )
endif()
