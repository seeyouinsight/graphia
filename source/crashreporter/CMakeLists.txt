include(${CMAKE_CURRENT_SOURCE_DIR}/../common.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/thirdparty_headers.cmake)

add_definitions(-DPRODUCT_NAME="${PROJECT_NAME}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/report.h
    ${CMAKE_CURRENT_LIST_DIR}/../app/rendering/openglfunctions.h
    ${CMAKE_CURRENT_LIST_DIR}/../shared/utils/preferences.h
    ${CMAKE_CURRENT_LIST_DIR}/../shared/utils/preferenceswatcher.h
    ${CMAKE_CURRENT_LIST_DIR}/../shared/utils/qmlpreferences.h
)

list(APPEND SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../app/rendering/openglfunctions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../shared/utils/preferences.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../shared/utils/preferenceswatcher.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../shared/utils/qmlpreferences.cpp
)

list(APPEND RESOURCES
    ${CMAKE_CURRENT_LIST_DIR}/resources.qrc
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(MSVC)
    add_executable(CrashReporter WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
else()
    add_executable(CrashReporter ${SOURCES} ${HEADERS} ${RESOURCES})
    install(TARGETS CrashReporter RUNTIME DESTINATION bin)
endif()

find_package(Qt5 COMPONENTS Qml Quick OpenGL OpenGLExtensions Xml REQUIRED)
target_link_libraries(CrashReporter
    Qt5::Qml
    Qt5::Quick
    Qt5::OpenGL
    Qt5::OpenGLExtensions
    Qt5::Xml
)

find_package(OpenGL REQUIRED)
target_link_libraries(CrashReporter ${OPENGL_gl_LIBRARY})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(CrashReporter Threads::Threads)

if(APPLE)
    target_link_libraries(CrashReporter "-framework CoreFoundation")
endif()

target_link_libraries(CrashReporter thirdparty_static)
