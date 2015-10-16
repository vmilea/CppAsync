file (GLOB _examples_cxx *.cpp)
file (GLOB _examples_h *.h)
file (GLOB _examples_util_cxx util/*.cpp)
file (GLOB _examples_util_h util/*.h)

source_group ("Examples" FILES ${_examples_cxx} ${_examples_h})
source_group ("Examples\\util" FILES ${_examples_util_cxx} ${_examples_util_h})

set (_examples_all_cxx ${_examples_cxx} ${_examples_util_cxx})

if (NOT (MINGW AND CMAKE_COMPILER_IS_GNUCXX))
	add_precompiled_header (stdafx.h _examples_all_cxx)
endif()

add_executable (Examples
	${_examples_cxx} ${_examples_h}
    ${_examples_util_cxx} ${_examples_util_h})

target_link_libraries (Examples ICppAsync)

message ("Boost linked libraries: ${Boost_LIBRARIES}")
target_link_libraries (Examples ${Boost_LIBRARIES})

if (OPENSSL_FOUND)
    message ("OpenSSL linked libraries: ${OPENSSL_LIBRARIES}")
    target_link_libraries (Examples ${OPENSSL_LIBRARIES})
endif()

if (WIN32)
    target_link_libraries (Examples ws2_32 mswsock)
elseif (UNIX)
    if (APPLE)
        target_link_libraries (Examples pthread)
    else()
        target_link_libraries (Examples rt pthread)
    endif()
endif()
