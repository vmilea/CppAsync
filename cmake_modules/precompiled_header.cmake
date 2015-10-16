# Generic support for precompiled headers
#
# Needs CMAKE_CURRENT_BINARY_DIR added to include path,
# this can be done with set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

function (ADD_PRECOMPILED_HEADER _headerFilename _srcList)

if (CMAKE_COMPILER_IS_GNUCXX)
	string (TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _cxxFlagsVarName)
	set (_args ${${_cxxFlagsVarName}})
	
	get_directory_property (_definitions DEFINITIONS)
	list (APPEND _args ${_definitions})

	get_directory_property (includeDirs INCLUDE_DIRECTORIES)
	foreach (includeDir ${includeDirs})
		list (APPEND _args "-I${includeDir}")
	endforeach()

	list (APPEND _args -x c++-header -c ${CMAKE_CURRENT_SOURCE_DIR}/${_headerFilename} -o ${_headerFilename}.gch)

	separate_arguments (_args)

	add_custom_command (OUTPUT ${_headerFilename}.gch
			COMMAND ${CMAKE_CXX_COMPILER} ${_args}
			DEPENDS ${_headerFilename})

	set_source_files_properties (${${_srcList}} PROPERTIES
			COMPILE_FLAGS "-include ${_headerFilename} -Winvalid-pch"
			OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_headerFilename}.gch)

elseif (MSVC)
	get_filename_component (_headerBasename ${_headerFilename} NAME_WE)

	set (pchFilename "${CMAKE_CURRENT_BINARY_DIR}/${_headerBasename}.pch")	

	foreach (src ${${_srcList}})
		get_filename_component (_srcBasename ${src} NAME_WE)

		if (_srcBasename STREQUAL _headerBasename)
			set_source_files_properties (${src} PROPERTIES
					COMPILE_FLAGS "/Yc\"${_headerFilename}\" /Fp\"${pchFilename}\""
					OBJECT_OUTPUTS ${pchFilename})
		else()
			set_source_files_properties (${src} PROPERTIES
					COMPILE_FLAGS "/Yu\"${_headerFilename}\" /FI\"${_headerFilename}\" /Fp\"${pchFilename}\""
					OBJECT_DEPENDS ${pchFilename})
		endif()
	endforeach()
endif()

endfunction()