# @file CoviseHelperMacros.cmake
#
# @author Blasius Czink
#
# Provides helper macros for build control:
#
#  ADD_COVISE_LIBRARY(targetname [STATIC | SHARED | MODULE] [EXCLUDE_FROM_ALL]
#              source1 source2 ... sourceN)
#    - covise specific wrapper macro of add_library. Please use this macro for covise libraries.
#
#  ADD_COVISE_EXECUTABLE(targetname [WIN32] [MACOSX_BUNDLE] [EXCLUDE_FROM_ALL]
#              source1 source2 ... sourceN)
#    - covise specific wrapper macro of add_executable. Please use this macro for covise executables.
#      Note: The variables SOURCES and HEADERS are added automatically.
#
#  ADD_COVISE_MODULE category targetname [WIN32] [MACOSX_BUNDLE] [EXCLUDE_FROM_ALL]
#              source1 source2 ... sourceN)
#    - covise specific wrapper macro of add_executable. Please use this macro for covise modules.
#      You should specify the category of the module. Passing an empty string for category will have the same effect
#      as using ADD_COVISE_EXECUTABLE()
#      Note: The variables SOURCES and HEADERS are added automatically.
#
#  COVISE_INSTALL_TARGET(targetname)
#    - covise specific wrapper macro of INSTALL(TARGETS ...) Please use this macro for installing covise targets.
#      You should use the cmake INSTALL() command for anything else but covise-targets.
#
#  ADD_COVISE_COMPILE_FLAGS(targetname flags)
#    - add additional compile_flags to the given target
#      Example: ADD_COVISE_COMPILE_FLAGS(coJS "-fPIC;-fno-strict-aliasing")
#
#  REMOVE_COVISE_COMPILE_FLAGS(targetname flags)
#    - remove compile flags from target
#      Example: REMOVE_COVISE_COMPILE_FLAGS(coJS "-fPIC")
#
#  ADD_COVISE_LINK_FLAGS(targetname flags)
#    - add additional link flags to the given target
#
#  REMOVE_COVISE_LINK_FLAGS(targetname flags)
#    - remove link flags from target
#
#  COVISE_WERROR(targetname)
#    - convenience macro to add "warnings-are-errors" flag to a given target
#      You may pass additional parameters after targetname...
#      Example: COVISE_WERROR(coJS WIN32)  -  will switch target "coJS" to "warnings-are-errors" on all windows versions
#               COVISE_WERROR(coJS BASEARCH yoroo zackel) - will switch target "coJS" to "warnings-are-errors" on
#                                                           yoroo, yorooopt, zackel and zackelopt 
#
#  COVISE_WNOERROR(targetname)
#    - convenience macro to remove "warnings-are-errors" flag from a given target
#      (syntax is equivalent to the above macro)
#
#  --------------------------------------------------------------------------------------
#
#  COVISE_ADJUST_LIB_VARS (basename)
#    - this is a support macro for cmake find modules (it simply sets/adjusts library-variables
#      depending on what was previously found for example by FIND_LIBRARY()
#
#  COVISE_RESET (varname)
#    - sets a variable to "not found"
#
#  COVISE_SET_FTPARAM (env_var_name env_var_value)
#    - set temporary environment variables for COVISE_TEST_FEATURE macro
#      (in case of libraries the debug versions will get filtered out)
#      Example: COVISE_SET_FTPARAM(EXAMPLE_FT_LIB "${MPICH_LIBRARIES}")
#
#  COVISE_TEST_FEATURE (feature_dest_var feature_test_name my_output)
#    - Compiles a small program given in "feature_test_name" and sets the variable "feature_dest_var"
#      if the compile/link process was successful
#      The full output from the compile/link process is returned in "my_output"
#      This macro expects the "feature-test"-files in CM_FEATURE_TESTS_DIR which is preset to
#      ${COVISEDIR}/cmake/FeatureTests
#
#      Example: COVISE_TEST_FEATURE(MPI_THREADED ft_mpi_threaded.c MY_OUTPUT)
#
#  COVISE_COPY_TARGET_PDB(target_name pdb_inst_prefix)
#    - gets the targets .pdb file and deploys it to the given location ${pdb_inst_prefix}/lib or
#      ${pdb_inst_prefix}/bin during install
#    - only the pdb files for debug versions of a library are installed
#      (this macro is windows specific)
# 
#  COVISE_DUMP_LIB_SETUP (basename)
#    - dumps the different library-variable contents to a file
#
#  USING(DEP1 DEP2 [optional])
#    - add dependencies DEP1 and DEP2,
#      all of them are optional, if 'optional' is present within the arguments
#
# @author Blasius Czink
#

# Macro to adjust library-variables
MACRO(COVISE_ADJUST_LIB_VARS basename)
  # if only the release version was found, set the debug variable also to the release version
  IF (${basename}_LIBRARY_RELEASE AND NOT ${basename}_LIBRARY_DEBUG)
    SET(${basename}_LIBRARY_DEBUG ${${basename}_LIBRARY_RELEASE} CACHE FILEPATH "${basename}_LIBRARY_DEBUG" FORCE)
    SET(${basename}_LIBRARY       ${${basename}_LIBRARY_RELEASE} CACHE FILEPATH "${basename}_LIBRARY" FORCE)
    SET(${basename}_LIBRARIES     ${${basename}_LIBRARY_RELEASE} CACHE FILEPATH "${basename}_LIBRARIES" FORCE)
  ENDIF (${basename}_LIBRARY_RELEASE AND NOT ${basename}_LIBRARY_DEBUG)

  # if only the debug version was found, set the release variable also to the debug version
  IF (${basename}_LIBRARY_DEBUG AND NOT ${basename}_LIBRARY_RELEASE)
    SET(${basename}_LIBRARY_RELEASE ${${basename}_LIBRARY_DEBUG} CACHE FILEPATH "${basename}_LIBRARY_RELEASE" FORCE)
    SET(${basename}_LIBRARY         ${${basename}_LIBRARY_DEBUG} CACHE FILEPATH "${basename}_LIBRARY" FORCE)
    SET(${basename}_LIBRARIES       ${${basename}_LIBRARY_DEBUG} CACHE FILEPATH "${basename}_LIBRARIES" FORCE)
  ENDIF (${basename}_LIBRARY_DEBUG AND NOT ${basename}_LIBRARY_RELEASE)
  
  # if both are set...
  IF (${basename}_LIBRARY_DEBUG AND ${basename}_LIBRARY_RELEASE)
    # if the generator supports configuration types then set
    # optimized and debug libraries, or if the CMAKE_BUILD_TYPE has a value
    IF (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)
      SET(${basename}_LIBRARY
          optimized ${${basename}_LIBRARY_RELEASE}
          debug     ${${basename}_LIBRARY_DEBUG}
      )
    ELSE(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)
      # if there are no configuration types and CMAKE_BUILD_TYPE has no value
      # then just use the release libraries
      SET(${basename}_LIBRARY ${${basename}_LIBRARY_RELEASE} )
    ENDIF(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)

    SET(${basename}_LIBRARIES
        optimized ${${basename}_LIBRARY_RELEASE}
        debug ${${basename}_LIBRARY_DEBUG}
        CACHE FILEPATH "${basename}_LIBRARIES" FORCE
    )
    
  ENDIF (${basename}_LIBRARY_DEBUG AND ${basename}_LIBRARY_RELEASE)

  SET(${basename}_LIBRARY "${${basename}_LIBRARY}" CACHE FILEPATH "The ${basename} library" FORCE)
  # do not show those in normal view
  MARK_AS_ADVANCED(FORCE
    ${basename}_LIBRARY
    ${basename}_LIBRARY_RELEASE
    ${basename}_LIBRARY_DEBUG
    ${basename}_LIBRARIES
  )
ENDMACRO(COVISE_ADJUST_LIB_VARS)


# helper to reset variables to ...-NOTFOUND so that the FIND...-macros run again
MACRO(COVISE_RESET varname)
  SET (${varname} "${varname}-NOTFOUND")
ENDMACRO(COVISE_RESET)

# helper to dump the lib-values to a simple text-file
MACRO(COVISE_DUMP_LIB_SETUP basename)
  SET (dump_file "${CMAKE_BINARY_DIR}/${basename}_lib_setup.txt")
  FILE(WRITE  ${dump_file} "${basename}_INCLUDE_DIR    = ${${basename}_INCLUDE_DIR}\n")
  FILE(APPEND ${dump_file} "${basename}_LIBRARY        = ${${basename}_LIBRARY}\n")
  FILE(APPEND ${dump_file} "${basename}_LIBRARY_RELESE = ${${basename}_LIBRARY_RELEASE}\n")
  FILE(APPEND ${dump_file} "${basename}_LIBRARY_DEBUG  = ${${basename}_LIBRARY_DEBUG}\n")
  FILE(APPEND ${dump_file} "${basename}_LIBRARIES      = ${${basename}_LIBRARIES}\n")
ENDMACRO(COVISE_DUMP_LIB_SETUP)

# helper to print the lib-values to a simple text-file
MACRO(COVISE_PRINT_LIB_SETUP basename)
  MESSAGE("${basename}_INCLUDE_DIR    = ${${basename}_INCLUDE_DIR}")
  MESSAGE("${basename}_LIBRARY        = ${${basename}_LIBRARY}")
  MESSAGE("${basename}_LIBRARY_RELESE = ${${basename}_LIBRARY_RELEASE}")
  MESSAGE("${basename}_LIBRARY_DEBUG  = ${${basename}_LIBRARY_DEBUG}")
  MESSAGE("${basename}_LIBRARIES      = ${${basename}_LIBRARIES}")
ENDMACRO(COVISE_PRINT_LIB_SETUP)

MACRO(COVISE_SET_FTPARAM env_var_name env_var_value)
  SET(FOUT "")

  #MESSAGE("orig mystring is ${env_var_value}")

  # get rid of debug libs RegExp ([Dd][Ee][Bb][Uu][Gg];[^;]*;?)
  STRING (REGEX REPLACE "[Dd][Ee][Bb][Uu][Gg];[^;]*;?" "" FOUT "${env_var_value}")
  #MESSAGE("1.mystring is ${FOUT}")

  # get rid of "optimized"
  STRING (REPLACE "optimized;" "" FOUT "${FOUT}")
  #MESSAGE("2.mystring is ${FOUT}")

  # add backslash before ";"
  STRING (REPLACE ";" "\\;" FOUT "${FOUT}")
  #MESSAGE("3.mystring is ${FOUT}")

  SET(ENV{${env_var_name}} "${FOUT}")
ENDMACRO(COVISE_SET_FTPARAM)


MACRO(COVISE_CLEAN_DEBUG_LIBS var_name var_value)
  SET (FOUT "")
  # get rid of debug libs RegExp ([Dd][Ee][Bb][Uu][Gg];[^;]*;?)
  STRING (REGEX REPLACE "[Dd][Ee][Bb][Uu][Gg];[^;]*;?" "" FOUT "${var_value}")
  # get rid of "optimized"
  STRING (REPLACE "optimized;" "" FOUT "${FOUT}")
  # replace backslash with space
  STRING (REPLACE ";" " " FOUT "${FOUT}")
  SET(${var_name} "${FOUT}")
ENDMACRO(COVISE_CLEAN_DEBUG_LIBS)


MACRO(COVISE_TEST_FEATURE feature_dest_var feature_test_name my_output)
  MESSAGE (STATUS "Checking for ${feature_test_name}")
  TRY_COMPILE (${feature_dest_var}
               ${CMAKE_BINARY_DIR}/CMakeTemp
               ${CM_FEATURE_TESTS_DIR}/${feature_test_name}
               CMAKE_FLAGS
               -DINCLUDE_DIRECTORIES=$ENV{COVISE_FT_INC}
               -DLINK_LIBRARIES=$ENV{COVISE_FT_LIB}
               OUTPUT_VARIABLE ${my_output}
  )
  # Feature test failed
  IF (${feature_dest_var})
     MESSAGE (STATUS "Checking for ${feature_test_name} - succeeded")
  ELSE (${feature_dest_var})
     MESSAGE (STATUS "Checking for ${feature_test_name} - feature not available")
  ENDIF (${feature_dest_var})
ENDMACRO(COVISE_TEST_FEATURE)


FUNCTION(COVISE_COPY_TARGET_PDB target_name pdb_inst_prefix)
  IF(MSVC)
    GET_TARGET_PROPERTY(TARGET_LOC ${target_name} DEBUG_LOCATION)
    IF(TARGET_LOC)
      GET_FILENAME_COMPONENT(TARGET_HEAD "${TARGET_LOC}" NAME_WE)
      # GET_FILENAME_COMPONENT(FNABSOLUTE  "${TARGET_LOC}" ABSOLUTE)
      GET_FILENAME_COMPONENT(TARGET_PATH "${TARGET_LOC}" PATH)
      SET(TARGET_PDB "${TARGET_PATH}/${TARGET_HEAD}.pdb")
      # MESSAGE(STATUS "PDB-File of ${target_name} is ${TARGET_PDB}")
      GET_TARGET_PROPERTY(TARGET_TYPE ${target_name} TYPE)
      IF(TARGET_TYPE)
        SET(pdb_dest )
        IF(${TARGET_TYPE} STREQUAL "STATIC_LIBRARY")
          SET(pdb_dest lib)
        ELSE(${TARGET_TYPE} STREQUAL "STATIC_LIBRARY")
          SET(pdb_dest bin)
        ENDIF(${TARGET_TYPE} STREQUAL "STATIC_LIBRARY")
        # maybe an optional category path given?
        IF(NOT ARGN STREQUAL "")
          SET(category_path "${ARGV2}")
        ENDIF(NOT ARGN STREQUAL "")
        INSTALL(FILES ${TARGET_PDB} DESTINATION "${pdb_inst_prefix}/${pdb_dest}${category_path}" CONFIGURATIONS "Debug") 
      ENDIF(TARGET_TYPE)
    ENDIF(TARGET_LOC)
  ENDIF(MSVC)
ENDFUNCTION(COVISE_COPY_TARGET_PDB)


MACRO(COVISE_INVERT_BOOL var)
  IF(${var})
    SET(${var} FALSE)
  ELSE(${var})
    SET(${var} TRUE)
  ENDIF(${var})
ENDMACRO(COVISE_INVERT_BOOL)


MACRO(COVISE_LIST_CONTAINS var value)
  SET(${var})
  FOREACH (value2 ${ARGN})
    STRING (TOUPPER ${value}  str1)
    STRING (TOUPPER ${value2} str2)
    IF (${str1} STREQUAL ${str2})
      SET (${var} TRUE)
    ENDIF (${str1} STREQUAL ${str2})
  ENDFOREACH (value2)
ENDMACRO(COVISE_LIST_CONTAINS)


MACRO(COVISE_LIST_CONTAINS_CS var value)
  SET (${var})
  FOREACH (value2 ${ARGN})
    IF (${value} STREQUAL ${value2})
      SET (${var} TRUE)
    ENDIF (${value} STREQUAL ${value2})
  ENDFOREACH (value2)
ENDMACRO(COVISE_LIST_CONTAINS_CS)


MACRO(COVISE_MSVC_RUNTIME_OPTION)
  IF(MSVC)
    OPTION (MSVC_USE_STATIC_RUNTIME "Use static MS-Runtime (/MT, /MTd)" OFF)
    IF(MSVC_USE_STATIC_RUNTIME)
      FOREACH(FLAG_VAR CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
                        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
      IF(${FLAG_VAR} MATCHES "/MD")
        STRING(REGEX REPLACE "/MD" "/MT" BCMSVC_${FLAG_VAR} "${${FLAG_VAR}}")
        SET(${FLAG_VAR} ${BCMSVC_${FLAG_VAR}} CACHE STRING "" FORCE)
      ENDIF(${FLAG_VAR} MATCHES "/MD")
      ENDFOREACH(FLAG_VAR)
    ELSE(MSVC_USE_STATIC_RUNTIME)
      FOREACH(FLAG_VAR CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
                        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
      IF(${FLAG_VAR} MATCHES "/MT")
        STRING(REGEX REPLACE "/MT" "/MD" BCMSVC_${FLAG_VAR} "${${FLAG_VAR}}")
        SET(${FLAG_VAR} ${BCMSVC_${FLAG_VAR}} CACHE STRING "" FORCE)
      ENDIF(${FLAG_VAR} MATCHES "/MT")
      ENDFOREACH(FLAG_VAR)
    ENDIF(MSVC_USE_STATIC_RUNTIME)
  ENDIF(MSVC)
ENDMACRO(COVISE_MSVC_RUNTIME_OPTION)

MACRO(COVISE_HAS_PREPROCESSOR_ENTRY CONTENTS KEYWORD VARIABLE)
  STRING(REGEX MATCH "\n *# *define +(${KEYWORD})" PREPROC_TEMP_VAR ${${CONTENTS}})
  
  IF (CMAKE_MATCH_1)
    SET(${VARIABLE} TRUE)
  ELSE (CMAKE_MATCH_1)
    set(${VARIABLE} FALSE)
  ENDIF (CMAKE_MATCH_1)
  
ENDMACRO(COVISE_HAS_PREPROCESSOR_ENTRY)

# Macro to adjust the output directories of a target
FUNCTION(COVISE_ADJUST_OUTPUT_DIR targetname)
  GET_TARGET_PROPERTY(TARGET_TYPE ${targetname} TYPE)
  IF(TARGET_TYPE)
    # MESSAGE("COVISE_ADJUST_OUTPUT_DIR(${targetname}) : TARGET_TYPE = ${TARGET_TYPE}")
    IF(TARGET_TYPE STREQUAL EXECUTABLE)
      SET(BINLIB_SUFFIX "bin")
    ELSE()
      SET(BINLIB_SUFFIX "lib")
    ENDIF()
    # optional binlib override
    IF(NOT ${ARGV2} STREQUAL "")
      SET(BINLIB_SUFFIX ${ARGV2})
    ENDIF()    

    SET(MYPATH_POSTFIX )
    # optional path-postfix specified?
    IF(NOT ${ARGV1} STREQUAL "")
      IF("${ARGV1}" MATCHES "^/.*")
        SET(MYPATH_POSTFIX "${ARGV1}")
      ELSE()
        SET(MYPATH_POSTFIX "/${ARGV1}")
      ENDIF()
    ENDIF()
    
    # adjust
    IF(CMAKE_CONFIGURATION_TYPES)
      # generator supports configuration types
      FOREACH(conf_type ${CMAKE_CONFIGURATION_TYPES})
        STRING(TOUPPER "${conf_type}" upper_conf_type_str)
        IF(COVISE_DESTDIR)
          IF(upper_conf_type_str STREQUAL "DEBUG")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_conf_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_conf_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_conf_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          ELSE(upper_conf_type_str STREQUAL "DEBUG")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_conf_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_conf_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_conf_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          ENDIF(upper_conf_type_str STREQUAL "DEBUG")
        ELSE(COVISE_DESTDIR)
          IF(upper_conf_type_str STREQUAL "DEBUG")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_conf_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_conf_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_conf_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          ELSE(upper_conf_type_str STREQUAL "DEBUG")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_conf_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_conf_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
            SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_conf_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          ENDIF(upper_conf_type_str STREQUAL "DEBUG")
        ENDIF(COVISE_DESTDIR)
      ENDFOREACH(conf_type)
    ELSE(CMAKE_CONFIGURATION_TYPES)
      # no configuration types - probably makefile generator
      STRING(TOUPPER "${CMAKE_BUILD_TYPE}" upper_build_type_str)
      IF(COVISE_DESTDIR)
        IF(upper_build_type_str STREQUAL "DEBUG")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        ELSE(upper_build_type_str STREQUAL "DEBUG")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        ENDIF(upper_build_type_str STREQUAL "DEBUG")
      ELSE(COVISE_DESTDIR)
        IF(upper_build_type_str STREQUAL "DEBUG")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        ELSE(upper_build_type_str STREQUAL "DEBUG")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
          SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${DBG_ARCHSUFFIX}opt/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        ENDIF(upper_build_type_str STREQUAL "DEBUG")
      ENDIF(COVISE_DESTDIR)
    ENDIF(CMAKE_CONFIGURATION_TYPES)
    
  ELSE(TARGET_TYPE)
    MESSAGE("COVISE_ADJUST_OUTPUT_DIR() - Could not retrieve target type of ${targetname}")
  ENDIF(TARGET_TYPE)
ENDFUNCTION(COVISE_ADJUST_OUTPUT_DIR)

# Macro to add covise libraries
MACRO(ADD_COVISE_LIBRARY targetname)
  ADD_LIBRARY(${ARGV} ${SOURCES} ${HEADERS})
  # SET_TARGET_PROPERTIES(${targetname} PROPERTIES PROJECT_LABEL "${targetname}")
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES OUTPUT_NAME "${targetname}")

  set_target_properties(${targetname} PROPERTIES FOLDER "Kernel_Libraries")
  COVISE_ADJUST_OUTPUT_DIR(${targetname})
  
  # set additional COVISE_COMPILE_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES COMPILE_FLAGS "${COVISE_COMPILE_FLAGS}")
  # set additional COVISE_LINK_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES LINK_FLAGS "${COVISE_LINK_FLAGS}")
  
  #SET_TARGET_PROPERTIES(${targetname} PROPERTIES DEBUG_OUTPUT_NAME "${targetname}${CMAKE_DEBUG_POSTFIX}")
  #SET_TARGET_PROPERTIES(${targetname} PROPERTIES RELEASE_OUTPUT_NAME "${targetname}${CMAKE_RELEASE_POSTFIX}")
  #SET_TARGET_PROPERTIES(${targetname} PROPERTIES RELWITHDEBINFO_OUTPUT_NAME "${targetname}${CMAKE_RELWITHDEBINFO_POSTFIX}")
  #SET_TARGET_PROPERTIES(${targetname} PROPERTIES MINSIZEREL_OUTPUT_NAME "${targetname}${CMAKE_MINSIZEREL_POSTFIX}")
  
  UNSET(SOURCES)
  UNSET(HEADERS)

  COVISE_EXPORT_TARGET(${targetname})
ENDMACRO(ADD_COVISE_LIBRARY)

MACRO(COVISE_ADD_LIBRARY)
   ADD_COVISE_LIBRARY(${ARGN})
ENDMACRO(COVISE_ADD_LIBRARY)

# Macro to add covise executables
MACRO(ADD_COVISE_EXECUTABLE targetname)
  ADD_EXECUTABLE(${targetname} ${ARGN} ${SOURCES} ${HEADERS})
  # SET_TARGET_PROPERTIES(${targetname} PROPERTIES PROJECT_LABEL "${targetname}")
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES OUTPUT_NAME "${targetname}")
  
  set_target_properties(${targetname} PROPERTIES FOLDER "Executables")
  COVISE_ADJUST_OUTPUT_DIR(${targetname})
  
  # set additional COVISE_COMPILE_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES COMPILE_FLAGS "${COVISE_COMPILE_FLAGS}")
  # set additional COVISE_LINK_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES LINK_FLAGS "${COVISE_LINK_FLAGS}")
  
  # SET_TARGET_PROPERTIES(${targetname} PROPERTIES DEBUG_OUTPUT_NAME "${targetname}${CMAKE_DEBUG_POSTFIX}")
  UNSET(SOURCES)
  UNSET(HEADERS)
ENDMACRO(ADD_COVISE_EXECUTABLE)

# Macro to add covise modules (executables with a module-category)
MACRO(ADD_COVISE_MODULE category targetname)
  ADD_EXECUTABLE(${targetname} ${ARGN} ${SOURCES} ${HEADERS})
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES OUTPUT_NAME "${targetname}")
  
  set_target_properties(${targetname} PROPERTIES FOLDER ${category}_Modules)
  COVISE_ADJUST_OUTPUT_DIR(${targetname} ${category})
  
  # set additional COVISE_COMPILE_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES COMPILE_FLAGS "${COVISE_COMPILE_FLAGS}")
  # set additional COVISE_LINK_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES LINK_FLAGS "${COVISE_LINK_FLAGS}")
  # use the LABELS property on the target to save the category
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES LABELS "${category}")
  
  # SET_TARGET_PROPERTIES(${targetname} PROPERTIES DEBUG_OUTPUT_NAME "${targetname}${CMAKE_DEBUG_POSTFIX}")
  UNSET(SOURCES)
  UNSET(HEADERS)
ENDMACRO(ADD_COVISE_MODULE)

MACRO(covise_add_module category targetname)
   add_covise_module(${category} ${targetname} ${ARGN})
   target_link_libraries(${targetname} coApi coAppl coCore coAlg)
ENDMACRO(covise_add_module)

MACRO(COVER_ADD_PLUGIN targetname)
 set(LIBS "")
  foreach(f ${ARGN})
     get_filename_component(ext ${f} EXT)
     if(ext MATCHES "\\.(h|hpp|hxx|inl|inc)\$")
        #message("Header: ${f} - ${ext}")
        set(HEADERS ${HEADERS} ${f})
     elseif(ext MATCHES "\\.(c|cu|cpp|cxx|mm)\$")
        #message("Source: ${f} - ${ext}")
        set(SOURCES ${SOURCES} ${f})
     elseif(ext MATCHES "\\.(obj)\$")
        #message("Obj: ${f} - ${ext}") 
		#don#t add obj files as lib
        set(OBJECTS ${OBJECTS} ${f})
     else()
        #message("Lib: ${f} - ${ext}")
        set(LIBS ${LIBS} ${f})
     endif()
  endforeach()
  
  COVER_ADD_PLUGIN_TARGET(${targetname} ${OBJECTS})

 

  TARGET_LINK_LIBRARIES(${targetname} ${LIBS})
  IF(DEFINED EXTRA_LIBS)
     TARGET_LINK_LIBRARIES(${targetname} ${EXTRA_LIBS})
  ENDIF()
  if(CUDA_FOUND AND COVISE_USE_CUDA)
     TARGET_LINK_LIBRARIES(${targetname} ${CUDA_LIBRARIES})
  endif()
  COVER_INSTALL_PLUGIN(${targetname})
ENDMACRO(COVER_ADD_PLUGIN)

# Macro to add OpenCOVER plugins
MACRO(COVER_ADD_PLUGIN_TARGET targetname)
  IF(NOT OPENSCENEGRAPH_FOUND)
    RETURN()
  ENDIF()
  USING(VTK optional)
  
  IF(WIN32)
    ADD_DEFINITIONS(-DIMPORT_PLUGIN)
  ENDIF(WIN32)

  INCLUDE_DIRECTORIES(
    ${ZLIB_INCLUDE_DIR}
    ${JPEG_INCLUDE_DIR}
    ${PNG_INCLUDE_DIR}
    ${TIFF_INCLUDE_DIR}
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
    "${COVISEDIR}/src/OpenCOVER"
  )
IF(APPLE)
  ADD_LIBRARY(${targetname} MODULE ${ARGN} ${SOURCES} ${HEADERS})
ELSE(APPLE)
  ADD_LIBRARY(${targetname} SHARED ${ARGN} ${SOURCES} ${HEADERS})
ENDIF(APPLE)
  
  set_target_properties(${targetname} PROPERTIES FOLDER "OpenCOVER_Plugins")
  
  # SET_TARGET_PROPERTIES(${targetname} PROPERTIES PROJECT_LABEL "${targetname}")
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES OUTPUT_NAME "${targetname}")

  COVISE_ADJUST_OUTPUT_DIR(${targetname} "OpenCOVER/plugins")
  
  # set additional COVISE_COMPILE_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES COMPILE_FLAGS "${COVISE_COMPILE_FLAGS}")
  # set additional COVISE_LINK_FLAGS
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES LINK_FLAGS "${COVISE_LINK_FLAGS}")
  # switch off "lib" prefix for MinGW
  IF(MINGW)
    SET_TARGET_PROPERTIES(${targetname} PROPERTIES PREFIX "")
  ENDIF(MINGW)
    
  TARGET_LINK_LIBRARIES(${targetname} coOpenPluginUtil coOpenCOVER coOpenVRUI coOSGVRUI
     ${COVISE_VRBCLIENT_LIBRARY} ${COVISE_CONFIG_LIBRARY} ${COVISE_UTIL_LIBRARY}
     ${OPENSCENEGRAPH_LIBRARIES}) # ${CMAKE_THREAD_LIBS_INIT})
  
  IF(APPLE)
    ADD_COVISE_LINK_FLAGS(${targetname} "-undefined error")
    ADD_COVISE_LINK_FLAGS(${targetname} "-flat_namespace")
  ENDIF(APPLE)
  
  UNSET(SOURCES)
  UNSET(HEADERS)

  qt_use_modules(${targetname} Core)
ENDMACRO(COVER_ADD_PLUGIN_TARGET)

MACRO(COVISE_INSTALL_DEPENDENCIES targetname)
  IF (${targetname}_LIB_DEPENDS)
     IF(WIN32)
	 SET(upper_build_type_str "RELEASE")
     ELSE(WIN32)
        STRING(TOUPPER "${CMAKE_BUILD_TYPE}" upper_build_type_str)
	 ENDIF(WIN32)
     # Get dependencies
     SET(depends "${targetname}_LIB_DEPENDS")
#    MESSAGE(${${depends}})
     LIST(LENGTH ${depends} len)
     MATH(EXPR len "${len} - 2")
#     MESSAGE(${len})
     FOREACH(ctr RANGE 0 ${len} 2)
       # Split dependencies in mode (optimized, debug, general) and library name
       LIST(GET ${depends} ${ctr} mode)      
       MATH(EXPR ctr "${ctr} + 1")
       LIST(GET ${depends} ${ctr} value)
       STRING(TOUPPER ${mode} mode)
       # Check if the library is required for the current build type
       IF(${mode} STREQUAL "GENERAL")
         SET(check_install "1")
       ELSEIF(${mode} STREQUAL ${upper_build_type_str})
         SET(check_install "1")
       ELSE(${mode} STREQUAL "GENERAL")
         SET(check_install "0")
       ENDIF(${mode} STREQUAL "GENERAL")
       IF(${check_install})
         # If the library is from externlibs, pack it. 
         # FIXME: Currently only works with libraries added by FindXXX, as manually added 
         # libraries usually lack the full path.
         string(REPLACE "++" "\\+\\+" extlibs "$ENV{EXTERNLIBS}")
         IF (value MATCHES "^${extlibs}")
#           MESSAGE("VALUE+ ${value}")
           IF (IS_DIRECTORY ${value})
           ELSE (IS_DIRECTORY ${value})
           INSTALL(FILES ${value} DESTINATION ${ARCHSUFFIX}/lib)
           # Recurse symlinks
           # FIXME: Does not work with absolute links (that are evil anyway)
           WHILE(IS_SYMLINK ${value})
             EXECUTE_PROCESS(COMMAND readlink -n ${value} OUTPUT_VARIABLE link_target)
             GET_FILENAME_COMPONENT(link_dir ${value} PATH)
             SET(value "${link_dir}/${link_target}")
#             MESSAGE("VALUE ${value}")
             INSTALL(FILES ${value} DESTINATION ${ARCHSUFFIX}/lib)
           ENDWHILE(IS_SYMLINK ${value})
           ENDIF (IS_DIRECTORY ${value})
         ENDIF (value MATCHES "^${extlibs}")
       ENDIF(${check_install})
     ENDFOREACH(ctr)
  ENDIF (${targetname}_LIB_DEPENDS)
ENDMACRO(COVISE_INSTALL_DEPENDENCIES)

# Macro to export
MACRO(COVISE_EXPORT_TARGET targetname)
  IF(COVISE_EXPORT_TO_INSTALL)
     EXPORT(TARGETS ${ARGV} APPEND FILE "${COVISEDIR}/${ARCHSUFFIX}/${COVISE_EXPORTS_FILE}")
  ELSE(COVISE_EXPORT_TO_INSTALL)
    EXPORT(TARGETS ${ARGV} APPEND FILE "${CMAKE_BINARY_DIR}/${COVISE_EXPORTS_FILE}")
  ENDIF(COVISE_EXPORT_TO_INSTALL)
ENDMACRO(COVISE_EXPORT_TARGET)

# Macro to install and export
MACRO(COVISE_INSTALL_TARGET targetname)
  # was a category specified for the given target?
  GET_TARGET_PROPERTY(category ${targetname} LABELS)
  SET(_category_path )
  IF(category)
    SET(_category_path "/${category}")
  ENDIF(category)
  
  # @Florian: What are you trying to do? The following will create a subdirectory "/${ARCHSUFFIX}/..."
  # in each and every in-source subdirectory where you issue a COVISE_INSTALL_TARGET() !!!
  # cmake's INSTALL() will create the given subdirectories in ${CMAKE_INSTALL_PREFIX} at install time.
  #
#  IF (NOT EXISTS covise/${ARCHSUFFIX}/bin)
#    FILE(MAKE_DIRECTORY covise/${ARCHSUFFIX}/bin)
#  ENDIF(NOT EXISTS covise/${ARCHSUFFIX}/bin)
#  IF (NOT EXISTS covise/${ARCHSUFFIX}/bin${_category_path})
#    FILE(MAKE_DIRECTORY covise/${ARCHSUFFIX}/bin${_category_path})
#  ENDIF(NOT EXISTS covise/${ARCHSUFFIX}/bin${_category_path})
#  IF (NOT EXISTS covise/${ARCHSUFFIX}/lib)
#    FILE(MAKE_DIRECTORY covise/${ARCHSUFFIX}/lib)
#  ENDIF(NOT EXISTS covise/${ARCHSUFFIX}/lib)

  INSTALL(TARGETS ${ARGV} EXPORT covise-targets
          RUNTIME DESTINATION ${ARCHSUFFIX}/bin${_category_path}
          LIBRARY DESTINATION ${ARCHSUFFIX}/lib
          ARCHIVE DESTINATION ${ARCHSUFFIX}/lib
          COMPONENT modules.${category}
  )
  IF(COVISE_EXPORT_TO_INSTALL)
    INSTALL(EXPORT covise-targets DESTINATION ${ARCHSUFFIX}/lib COMPONENT modules.${category})
  ELSE(COVISE_EXPORT_TO_INSTALL)
    # EXPORT(TARGETS ${ARGV} APPEND FILE "${CMAKE_BINARY_DIR}/${BASEARCHSUFFIX}/${COVISE_EXPORTS_FILE}")
    #EXPORT(TARGETS ${ARGV} APPEND FILE "${COVISEDIR}/${ARCHSUFFIX}/${COVISE_EXPORTS_FILE}")
    EXPORT(TARGETS ${ARGV} APPEND FILE "${COVISEDIR}/${ARCHSUFFIX}/${COVISE_EXPORTS_FILE}")
  ENDIF(COVISE_EXPORT_TO_INSTALL)
  FOREACH(tgt ${ARGV})
    COVISE_COPY_TARGET_PDB(${tgt} ${ARCHSUFFIX} ${_category_path})
  ENDFOREACH(tgt)
  COVISE_INSTALL_DEPENDENCIES(${targetname})
ENDMACRO(COVISE_INSTALL_TARGET)

# Macro to install an OpenCOVER plugin
MACRO(COVER_INSTALL_PLUGIN targetname)
  INSTALL(TARGETS ${ARGV} EXPORT covise-targets
          RUNTIME DESTINATION ${ARCHSUFFIX}/lib/OpenCOVER/plugins
          LIBRARY DESTINATION ${ARCHSUFFIX}/lib/OpenCOVER/plugins
          ARCHIVE DESTINATION ${ARCHSUFFIX}/lib/OpenCOVER/plugins
          COMPONENT osgplugins.${category}
  )
  COVISE_INSTALL_DEPENDENCIES(${targetname})
ENDMACRO(COVER_INSTALL_PLUGIN)

#
# Per target flag handling
#

FUNCTION(ADD_COVISE_COMPILE_FLAGS targetname flags)
  GET_TARGET_PROPERTY(MY_CFLAGS ${targetname} COMPILE_FLAGS)
  IF(NOT MY_CFLAGS)
    SET(MY_CFLAGS "")
  ENDIF()
  FOREACH(cflag ${flags})
    #STRING(REGEX MATCH "${cflag}" flag_matched "${MY_CFLAGS}")
    #IF(NOT flag_matched)
      SET(MY_CFLAGS "${MY_CFLAGS} ${cflag}")
    #ENDIF(NOT flag_matched)
  ENDFOREACH(cflag)
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES COMPILE_FLAGS "${MY_CFLAGS}")
  # MESSAGE("added compile flags ${MY_CFLAGS} to target ${targetname}")
ENDFUNCTION(ADD_COVISE_COMPILE_FLAGS)

FUNCTION(REMOVE_COVISE_COMPILE_FLAGS targetname flags)
  GET_TARGET_PROPERTY(MY_CFLAGS ${targetname} COMPILE_FLAGS)
  IF(NOT MY_CFLAGS)
    SET(MY_CFLAGS "")
  ENDIF()
  FOREACH(cflag ${flags})
    STRING(REGEX REPLACE "${cflag}[ ]+|${cflag}$" "" MY_CFLAGS "${MY_CFLAGS}")
  ENDFOREACH(cflag)
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES COMPILE_FLAGS "${MY_CFLAGS}")
ENDFUNCTION(REMOVE_COVISE_COMPILE_FLAGS)

FUNCTION(ADD_COVISE_LINK_FLAGS targetname flags)
  GET_TARGET_PROPERTY(MY_LFLAGS ${targetname} LINK_FLAGS)
  IF(NOT MY_LFLAGS)
    SET(MY_LFLAGS "")
  ENDIF()
  FOREACH(lflag ${flags})
    #STRING(REGEX MATCH "${lflag}" flag_matched "${MY_LFLAGS}")
    #IF(NOT flag_matched)
      SET(MY_LFLAGS "${MY_LFLAGS} ${lflag}")
    #ENDIF(NOT flag_matched)
  ENDFOREACH(lflag)
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES LINK_FLAGS "${MY_LFLAGS}")
  #MESSAGE("added link flags ${MY_LFLAGS} to target ${targetname}")
ENDFUNCTION(ADD_COVISE_LINK_FLAGS)

FUNCTION(REMOVE_COVISE_LINK_FLAGS targetname flags)
  GET_TARGET_PROPERTY(MY_LFLAGS ${targetname} LINK_FLAGS)
  IF(NOT MY_LFLAGS)
    SET(MY_LFLAGS "")
  ENDIF()
  FOREACH(lflag ${flags})
    STRING(REGEX REPLACE "${lflag}[ ]+|${lflag}$" "" MY_LFLAGS "${MY_LFLAGS}")
  ENDFOREACH(lflag)
  SET_TARGET_PROPERTIES(${targetname} PROPERTIES LINK_FLAGS "${MY_LFLAGS}")
ENDFUNCTION(REMOVE_COVISE_LINK_FLAGS)

# small debug helper
FUNCTION(OUTPUT_COVISE_COMPILE_FLAGS targetname)
  GET_TARGET_PROPERTY(MY_CFLAGS ${targetname} COMPILE_FLAGS)
  MESSAGE("Target ${targetname} - COMPILE_FLAGS = ${MY_CFLAGS}")
ENDFUNCTION(OUTPUT_COVISE_COMPILE_FLAGS)

FUNCTION(OUTPUT_COVISE_LINK_FLAGS targetname)
  GET_TARGET_PROPERTY(MY_LFLAGS ${targetname} LINK_FLAGS)
  MESSAGE("Target ${targetname} - LINK_FLAGS = ${MY_LFLAGS}")
ENDFUNCTION(OUTPUT_COVISE_LINK_FLAGS)

# Set per target warnings-are-errors flag
FUNCTION(COVISE_WERROR targetname)
  # any archsuffixes or system names like (WIN32, UNIX, APPLE, MINGW etc.) passed as optional params?
  IF(${ARGC} GREATER 1)
    # we have optional stuff
    IF("${ARGV1}" STREQUAL "BASEARCH")
      # we expect BASEARCHSUFFIXES in the following params
      FOREACH(arch ${ARGN})
        IF("${arch}" STREQUAL "${BASEARCHSUFFIX}")
          ADD_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
        ENDIF("${arch}" STREQUAL "${BASEARCHSUFFIX}")
      ENDFOREACH(arch)
    ELSE("${ARGV1}" STREQUAL "BASEARCH")
      # we expect ONE additional param like WIN32, UNIX, APPLE, MSVC, MINGW etc.
      IF(${ARGV1})
        ADD_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
      ENDIF(${ARGV1})
    ENDIF("${ARGV1}" STREQUAL "BASEARCH")
  ELSE(${ARGC} GREATER 1)
    # only target is given
    ADD_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
  ENDIF(${ARGC} GREATER 1)
ENDFUNCTION(COVISE_WERROR)

# Remove per target warnings-are-errors flag
FUNCTION(COVISE_WNOERROR targetname)
  # any archsuffixes or system names like (WIN32, UNIX, APPLE, MINGW etc.) passed as optional params?
  IF(${ARGC} GREATER 1)
    # we have optional stuff
    IF(${ARGV1} STREQUAL "BASEARCH")
      # we expect BASEARCHSUFFIXES in the following params
      FOREACH(arch ${ARGN})
        IF("${arch}" STREQUAL "${BASEARCHSUFFIX}")
          REMOVE_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
        ENDIF("${arch}" STREQUAL "${BASEARCHSUFFIX}")
      ENDFOREACH(arch)
    ELSE(${ARGV1} STREQUAL "BASEARCH")
      # we expect ONE additional param like WIN32, UNIX, APPLE, MSVC, MINGW etc.
      IF(${ARGV1})
        REMOVE_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
      ENDIF(${ARGV1})
    ENDIF(${ARGV1} STREQUAL "BASEARCH")
  ELSE(${ARGC} GREATER 1)
    # only target is given
    REMOVE_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
  ENDIF(${ARGC} GREATER 1)
ENDFUNCTION(COVISE_WNOERROR)

FUNCTION(COVISE_NOWARN targetname)
  # any archsuffixes or system names like (WIN32, UNIX, APPLE, MINGW etc.) passed as optional params?
  IF(${ARGC} GREATER 1)
    # we have optional stuff
    IF(${ARGV1} STREQUAL "BASEARCH")
      # we expect BASEARCHSUFFIXES in the following params
      FOREACH(arch ${ARGN})
        IF("${arch}" STREQUAL "${BASEARCHSUFFIX}")
          REMOVE_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
          ADD_COVISE_COMPILE_FLAGS(${targetname} "-w")
        ENDIF("${arch}" STREQUAL "${BASEARCHSUFFIX}")
      ENDFOREACH(arch)
    ELSE(${ARGV1} STREQUAL "BASEARCH")
      # we expect ONE additional param like WIN32, UNIX, APPLE, MSVC, MINGW etc.
      IF(${ARGV1})
        REMOVE_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
        ADD_COVISE_COMPILE_FLAGS(${targetname} "-w")
      ENDIF(${ARGV1})
    ENDIF(${ARGV1} STREQUAL "BASEARCH")
  ELSE(${ARGC} GREATER 1)
    # only target is given
    REMOVE_COVISE_COMPILE_FLAGS(${targetname} "${COVISE_WERROR_FLAG}")
    ADD_COVISE_COMPILE_FLAGS(${targetname} "-w")
  ENDIF(${ARGC} GREATER 1)
ENDFUNCTION(COVISE_NOWARN)

MACRO(COVISE_UNFINISHED)
  STRING(REPLACE "${CMAKE_SOURCE_DIR}/" "" MYDIR "${CMAKE_CURRENT_SOURCE_DIR}")
  MESSAGE("Warning: Skipping unfinished CMakeLists.txt in ${MYDIR}")
  RETURN()
ENDMACRO(COVISE_UNFINISHED)

# ----------------
#  Unused stuff
# ----------------

# MACRO(COVISE_FILTER_OUT FILTERS INPUTS OUTPUT)
#   # Arguments:
#   #  FILTERS - list of patterns that need to be removed
#   #  INPUTS  - list of inputs that will be worked on
#   #  OUTPUT  - the filtered list to be returned
#   # 
#   # Example: 
#   #  SET(MYLIST this that and the other)
#   #  SET(FILTS this that)
#   #
#   #  FILTER_OUT("${FILTS}" "${MYLIST}" OUT)
#   #  MESSAGE("OUTPUT = ${OUT}")
#   #
#   # The output - 
#   #   OUTPUT = and;the;other
#   #
#   SET(FOUT "")
#   FOREACH(INP ${INPUTS})
#      SET(FILTERED 0)
#      FOREACH(FILT ${FILTERS})
#          IF(${FILTERED} EQUAL 0)
#              IF("${FILT}" STREQUAL "${INP}")
#                  SET(FILTERED 1)
#              ENDIF("${FILT}" STREQUAL "${INP}")
#          ENDIF(${FILTERED} EQUAL 0)
#      ENDFOREACH(FILT ${FILTERS})
#      IF(${FILTERED} EQUAL 0)
#          SET(FOUT ${FOUT} ${INP})
#      ENDIF(${FILTERED} EQUAL 0)
#   ENDFOREACH(INP ${INPUTS})
#   SET(${OUTPUT} ${FOUT})
# ENDMACRO(COVISE_FILTER_OUT)

# MACRO (CHECK_LIBRARY LIB_NAME LIB_DESC LIB_TEST_SOURCE)
#   SET (HAVE_${LIB_NAME})
#   MESSAGE (STATUS "Checking for ${LIB_DESC} library...")
#   TRY_COMPILE (HAVE_${LIB_NAME} ${CMAKE_BINARY_DIR}/.cmake_temp ${CMAKE_SOURCE_DIR}/cmake/${LIB_TEST_SOURCE})
#   IF (HAVE_${LIB_NAME})
#      # Don't need one
#      MESSAGE(STATUS "Checking for ${LIB_DESC} library... none needed")
#   ELSE (HAVE_${LIB_NAME})
#      # Try to find a suitable library
#      FOREACH (lib ${ARGN})
#          TRY_COMPILE (HAVE_${LIB_NAME} ${CMAKE_BINARY_DIR}/.cmake_temp
#                      ${CMAKE_SOURCE_DIR}/cmake/${LIB_TEST_SOURCE}
#                      CMAKE_FLAGS -DLINK_LIBRARIES:STRING=${lib})
#          IF (HAVE_${LIB_NAME})
#            MESSAGE (STATUS "Checking for ${LIB_DESC} library... ${lib}")
#            SET (HAVE_${LIB_NAME}_LIB ${lib})
#          ENDIF (HAVE_${LIB_NAME})
#      ENDFOREACH (lib)
#   ENDIF (HAVE_${LIB_NAME})
#   # Unable to find a suitable library
#   IF (NOT HAVE_${LIB_NAME})
#      MESSAGE (STATUS "Checking for ${LIB_DESC} library... not found")
#   ENDIF (NOT HAVE_${LIB_NAME})
# ENDMACRO (CHECK_LIBRARY)
# 
# ...the LIB_NAME param controls the name of the variable(s) that are set 
# if it finds the library. LIB_DESC is used for the status messages. An 
# example usage might be:
# 
# CHECK_LIBRARY(SCK socket scktest.c socket xnet ws2_32)


# Macro to link with COVISE and other libraries, COVISE libraries get their dependencies pulled in automatically
MACRO(COVISE_TARGET_LINK_LIBRARIES targetname libs)
   set(COUNT 0)
   foreach(A ${ARGV})
      if(NOT ${COUNT} EQUAL 0)
         if("${A}" STREQUAL "coAlg")
	    USE_VTK(OPTIONAL)
         endif("${A}" STREQUAL "coAlg")
      endif(NOT ${COUNT} EQUAL 0)
      math(EXPR COUNT "${COUNT}+1")
   endforeach(A ${ARGV})
   TARGET_LINK_LIBRARIES(${ARGV})
ENDMACRO(COVISE_TARGET_LINK_LIBRARIES)

MACRO(TESTIT)
ENDMACRO(TESTIT)

MACRO(USING_MESSAGE)
   #MESSAGE(${ARGN})
ENDMACRO(USING_MESSAGE)

MACRO(CREATE_USING)
  FIND_PROGRAM(GREP_EXECUTABLE grep PATHS $ENV{EXTERNLIBS}/UnixUtils DOC "grep executable")
  
  file(GLOB USING_FILES "${COVISEDIR}/cmake/Using/Use*.cmake")
  foreach(F ${USING_FILES})
     include(${F})
  endforeach(F ${USING_FILES})
  EXECUTE_PROCESS(COMMAND ${GREP_EXECUTABLE} -h USE_ ${USING_FILES}
     COMMAND ${GREP_EXECUTABLE} "^MACRO"
     OUTPUT_VARIABLE using_list)
  #message("using_list")
  #message("using_list ${using_list}")

  STRING(STRIP "${using_list}" using_list)

  STRING(REGEX REPLACE "MACRO\\([^_]*_\([^\n]*\)\\)" "\\1" using_list "${using_list}")
  STRING(REGEX REPLACE " optional" "" using_list "${using_list}")
  STRING(REGEX REPLACE "\n" ";" using_list "${using_list}")

  MESSAGE("USING list: ${using_list}")

  SET(filename "${CMAKE_BINARY_DIR}/CoviseUsingMacros.cmake")
  LIST(LENGTH using_list using_list_size)
  MATH(EXPR using_list_size "${using_list_size} - 1")

  FILE(WRITE  ${filename} "MACRO(USING)\n\n")
  FILE(APPEND ${filename} "  SET(optional FALSE)\n")
  FILE(APPEND ${filename} "  STRING (REGEX MATCHALL \"(^|[^a-zA-Z0-9_])optional(\$|[^a-zA-Z0-9_])\" optional \"\$")
  FILE(APPEND ${filename} "{ARGV}\")\n\n")
  FILE(APPEND ${filename} "  FOREACH(use \$")
  FILE(APPEND ${filename} "{ARGV})\n\n")
  FILE(APPEND ${filename} "    STRING (TOUPPER \${use} use)\n")

  FOREACH(ctr RANGE ${using_list_size})
    LIST(GET using_list ${ctr} target_macro)
    FILE(APPEND ${filename} "    IF(use STREQUAL ${target_macro})\n")
    FILE(APPEND ${filename} "      USE_${target_macro}(\${optional})\n")
    FILE(APPEND ${filename} "    ENDIF(use STREQUAL ${target_macro})\n\n")
  ENDFOREACH(ctr)

  FILE(APPEND ${filename} "  ENDFOREACH(use)\n\n")
  FILE(APPEND ${filename} "ENDMACRO(USING)\n")

  INCLUDE(${filename})

ENDMACRO(CREATE_USING)

# use this instead of FIND_PACKAGE to prefer Package in $PACKAGE_HOME and $EXTERNLIBS/package
MACRO(COVISE_FIND_PACKAGE package)
   SET(SAVED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})

   STRING(TOUPPER ${package} UPPER)
   STRING(TOLOWER ${package} LOWER)
   IF(MINGW)
      SET(CMAKE_PREFIX_PATH ${MINGW_SYSROOT} ${CMAKE_PREFIX_PATH})
   ENDIF()
   IF(NOT "$ENV{EXTERNLIBS}" STREQUAL "")
      SET(CMAKE_PREFIX_PATH $ENV{EXTERNLIBS}/${LOWER}/bin ${CMAKE_PREFIX_PATH})
      SET(CMAKE_PREFIX_PATH $ENV{EXTERNLIBS} ${CMAKE_PREFIX_PATH})
      SET(CMAKE_PREFIX_PATH $ENV{EXTERNLIBS}/${LOWER} ${CMAKE_PREFIX_PATH})
   ENDIF()
   IF(NOT "$ENV{${UPPER}_HOME}" STREQUAL "")
      SET(CMAKE_PREFIX_PATH $ENV{${UPPER}_HOME} ${CMAKE_PREFIX_PATH})
   ENDIF()
   #message("looking for package ${ARGV}")
   #message("CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
   FIND_PACKAGE(${ARGV})

   SET(CMAKE_PREFIX_PATH ${SAVED_CMAKE_PREFIX_PATH})
ENDMACRO(COVISE_FIND_PACKAGE PACKAGE)

MACRO(COVISE_USE_OPENMP target)
   IF(UNIX OR MINGW)
      SET(WITH_OPENMP "TRUE")
      IF(MINGW)
         SET(WITH_OPENMP "FALSE")
      ENDIF()
      IF(APPLE)
         # with cmake 2.8.8 (rc for now), clang will be identifiable
         # using CMAKE_COMPILER_ID. For now, use this hack to identify
         # clang on Max OS X
         EXEC_PROGRAM(${CMAKE_CXX_COMPILER} ARGS --version OUTPUT_VARIABLE _clang_version_info)
         IF(${_clang_version_info} MATCHES "clang")
            SET(WITH_OPENMP "FALSE")
         ENDIF(${_clang_version_info} MATCHES "clang")
      ENDIF(APPLE)

      IF(WITH_OPENMP)
         ADD_COVISE_COMPILE_FLAGS(${target} "-fopenmp")
         ADD_COVISE_LINK_FLAGS(${target} "-fopenmp")
      ENDIF()
   ENDIF(UNIX OR MINGW)

   IF(MSVC)
      ADD_COVISE_COMPILE_FLAGS(${target} "/openmp")
   ENDIF(MSVC)
ENDMACRO(COVISE_USE_OPENMP)

MACRO(COVISE_FIND_CUDA)
   COVISE_FIND_PACKAGE(CUDA) # optional
   IF(CUDA_FOUND AND COVISE_USE_CUDA)
      ADD_DEFINITIONS(-DHAVE_CUDA)
      if(APPLE)
          set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "\"-DBOOST_NOINLINE=__attribute__ ((noinline))\"")
      endif()
      if("${CUDA_VERSION}" VERSION_LESS 7.0)
          if(APPLE)
              set(CUDA_HOST_COMPILER ${COVISEDIR}/scripts/cuda-host-compiler CACHE STRING "CUDA nvcc host compiler" FORCE)
          elseif(${BASEARCHSUFFIX} STREQUAL "rhel6")
              set(CUDA_HOST_COMPILER ${COVISEDIR}/scripts/cuda-host-compiler CACHE STRING "CUDA nvcc host compiler" FORCE)
          elseif(${BASEARCHSUFFIX} STREQUAL "rhel7")
              SET(CUDA_HOST_COMPILER ${COVISEDIR}/scripts/cuda-host-compiler CACHE STRING "CUDA nvcc host compiler" FORCE)
          elseif(${BASEARCHSUFFIX} STREQUAL "tahr")
              SET(CUDA_HOST_COMPILER ${COVISEDIR}/scripts/cuda-host-compiler CACHE STRING "CUDA nvcc host compiler" FORCE)
          endif()
      else()
          if (APPLE)
              set(CUDA_PROPAGATE_HOST_FLAGS OFF)
          elseif(${BASEARCHSUFFIX} STREQUAL "rhel7")
              set(CUDA_HOST_COMPILER "/usr/bin/gcc" CACHE STRING "CUDA nvcc host compiler" FORCE)
          endif()
          if (COVISE_USE_CPP11)
              set(CUDA_NVCC_FLAGS "--std=c++11 ${CUDA_NVCC_FLAGS}")
          endif()
      endif()
   ENDIF(CUDA_FOUND AND COVISE_USE_CUDA)
ENDMACRO(COVISE_FIND_CUDA)
