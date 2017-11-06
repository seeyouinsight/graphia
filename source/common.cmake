set(CMAKE_CXX_STANDARD 14)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG)
endif()

# https://www.kdab.com/disabling-narrowing-conversions-in-signal-slot-connections/
add_definitions(-DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT)

add_definitions(-DQT_DEPRECATED_WARNINGS)
# disables all the APIs deprecated before Qt 6.0.0
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x060000)

add_definitions(-DAPP_URI="com.kajeka")
add_definitions(-DAPP_MINOR_VERSION=0)
add_definitions(-DAPP_MAJOR_VERSION=1)

if(MSVC)
    add_definitions(-DUNICODE -D_UNICODE)

    ENABLE_LANGUAGE(ASM_MASM)
    set(CMAKE_ASM_MASM_FLAGS "/nologo /D_M_X64 /W3 /Cx /Zi")
endif()

if (NOT "$ENV{VERSION}" STREQUAL "")
    set(Version $ENV{VERSION})
else()
    set(Version "development")
endif()

add_definitions(-DVERSION="${Version}")

if (NOT "$ENV{COPYRIGHT}" STREQUAL "")
    set(Copyright $ENV{COPYRIGHT})
else()
    set(Copyright "\(c\) Copyright notice")
endif()

add_definitions(-DCOPYRIGHT="${Copyright}")

include_directories(${CMAKE_CURRENT_LIST_DIR})
