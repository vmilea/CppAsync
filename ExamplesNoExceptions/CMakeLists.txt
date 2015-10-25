remove_definitions (-DHAVE_BOOST -DHAVE_BOOST_CONTEXT)
add_definitions (-DUT_NO_EXCEPTIONS)

if (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions (-fno-exceptions)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions (-fno-exceptions)
endif()

file (GLOB _noex_cxx *.cpp)
file (GLOB _noex_h *.h)
file (GLOB _noex_util_cxx ../Examples/util/*.cpp)
file (GLOB _noex_util_h ../Examples/util/*.h)

source_group ("ExamplesNoException" FILES ${_noex_cxx} ${_noex_h})
source_group ("ExamplesNoException\\util" FILES ${_noex_util_cxx} ${_noex_util_h})

set (_noex_all_cxx ${_noex_cxx} ${_noex_util_cxx})

if (NOT (MINGW AND CMAKE_COMPILER_IS_GNUCXX))
    add_precompiled_header (stdafx.h _noex_all_cxx)
endif()

add_executable (ExamplesNoExceptions
    ${_noex_cxx} ${_noex_h}
    ${_noex_util_cxx} ${_noex_util_h})

target_link_libraries (ExamplesNoExceptions ICppAsync)

if (UNIX)
    if (APPLE)
        target_link_libraries (ExamplesNoExceptions pthread)
    else()
        target_link_libraries (ExamplesNoExceptions rt pthread)
    endif()
endif()
