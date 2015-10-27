cmake_minimum_required (VERSION 3.1)

project (CppAsyncSolution CXX)

#
# modules
#

set (CMAKE_MODULE_PATH ${CppAsyncSolution_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})

include (precompiled_header)
include (enable_max_warning_level)

#
# Boost
#

set (Boost_USE_STATIC_LIBS ON)
set (Boost_USE_STATIC_RUNTIME OFF)

set (_boost_deps context thread system)

message ("Looking for Boost libraries: ${_boost_deps}")
find_package (Boost 1.58.0 COMPONENTS ${_boost_deps})

if (Boost_FOUND)
    message ("Using Boost ${Boost_LIB_VERSION} from ${Boost_INCLUDE_DIRS}")
    set (BoostContext_FOUND YES)
else()
    list(REMOVE_ITEM _boost_deps context)
    message ("Fall back, looking for: ${_boost_deps}")
    find_package (Boost 1.58.0 COMPONENTS ${_boost_deps})

    if (Boost_FOUND)
        message ("Using Boost ${Boost_LIB_VERSION} from ${Boost_INCLUDE_DIRS}")
        message ("Boost.Context not found, stackful examples will be skipped.")
    else()
        message ("Boost not found, some examples will be skipped.")
    endif()
endif()

if (Boost_FOUND)
    include_directories (${Boost_INCLUDE_DIRS})

    # disable Boost autolinking
    add_definitions (-DBOOST_ALL_NO_LIB)

    add_definitions (-DHAVE_BOOST)

    if (BoostContext_FOUND)
        add_definitions (-DHAVE_BOOST_CONTEXT)
    endif()

    #
    # OpenSSL
    #

    find_package (OpenSSL)

    if (OPENSSL_FOUND)
        message ("Using OpenSSL ${OPENSSL_VERSION} from ${OPENSSL_INCLUDE_DIR}")
        include_directories (${OPENSSL_INCLUDE_DIR})

        # enable OpenSSL examples
        add_definitions (-DHAVE_OPENSSL)
    else()
        message ("OpenSSL not found, some examples will be skipped.")
    endif()
endif()

#
# defs
#

if (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions (-std=c++11 -fstrict-aliasing -Wstrict-aliasing)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions (-std=c++11 -fstrict-aliasing -Wstrict-aliasing)
elseif (MSVC)
    add_definitions (/wd4100 /wd4127 /wd4913)

    if (NOT (MSVC_VERSION LESS 1900))
        # enable support for resumable functions (N4402, N4403)
        add_definitions (/await)

        # run-time error checks don't work yet in combination with /await
        STRING (REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    endif()
endif()

if (MINGW AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_definitions (-march=i686)
endif()

if (MSVC AND BoostContext_FOUND)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
endif()

#
# paths
#

set (CMAKE_INCLUDE_CURRENT_DIR TRUE)

#
# subprojects
#

add_subdirectory (CppAsync)
add_subdirectory (Examples)
add_subdirectory (ExamplesNoExceptions)
