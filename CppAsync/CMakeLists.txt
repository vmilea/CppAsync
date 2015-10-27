file (GLOB _cppasync_h *.h)
file (GLOB _cppasync_asio_h asio/*.h)
file (GLOB _cppasync_experimental_h experimental/*.h)
file (GLOB _cppasync_impl_h impl/*.h)
file (GLOB _cppasync_util_h util/*.h)

source_group ("CppAsync" FILES ${_cppasync_h})
source_group ("CppAsync\\asio" FILES ${_cppasync_asio_h})
source_group ("CppAsync\\experimental" FILES ${_cppasync_experimental_h})
source_group ("CppAsync\\impl" FILES ${_cppasync_impl_h})
source_group ("CppAsync\\util" FILES ${_cppasync_util_h})

set (_cpp_async_all_h ${_cppasync_h}
    ${_cppasync_asio_h} ${_cppasync_experimental_h} ${_cppasync_impl_h} ${_cppasync_util_h})

add_library (ICppAsync INTERFACE)
target_include_directories (ICppAsync INTERFACE ../)

#target_sources (CppAsync INTERFACE ${_cpp_async_all_h})

# add dummy target in order to show header files in project
add_custom_target (CppAsync SOURCES ${_cpp_async_all_h})
