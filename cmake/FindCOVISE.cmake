# - Find COVISE

macro (GETENV_PATH var name)
   set(${var} $ENV{${name}})
   if (WIN32)
      string(REGEX REPLACE "\\\\" "/" ${var} "${${var}}")
   endif(WIN32)
endmacro (GETENV_PATH var name)

if(NOT $ENV{ARCHSUFFIX} STREQUAL "")
   set(COVISE_ARCHSUFFIX $ENV{ARCHSUFFIX})
else()
   message("COVISE: ARCHSUFFIX not set")
endif()

STRING(TOLOWER "${COVISE_ARCHSUFFIX}" COVISE_ARCHSUFFIX)
STRING(REGEX REPLACE "opt$" "" BASEARCHSUFFIX "${COVISE_ARCHSUFFIX}")
SET(DBG_ARCHSUFFIX "${BASEARCHSUFFIX}")
STRING(REGEX REPLACE "xenomai$" "" BASEARCHSUFFIX "${BASEARCHSUFFIX}")
STRING(REGEX REPLACE "mpi$" "" BASEARCHSUFFIX "${BASEARCHSUFFIX}")
#MESSAGE("COVISE: DBG_ARCHSUFFIX=${DBG_ARCHSUFFIX}")
#IF(COVISE_ARCHSUFFIX STREQUAL DBG_ARCHSUFFIX)
#  SET(CMAKE_BUILD_TYPE "Debug")
#ELSE()
#  SET(CMAKE_BUILD_TYPE "RelWithDebInfo")
#ENDIF()
#MESSAGE("COVISE: CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

if(NOT "$ENV{COVISEDIR}" STREQUAL "")
   getenv_path(COVISEDIR COVISEDIR)
else()
   message("COVISE: COVISEDIR not set")
endif()

if(NOT "$ENV{COVISEDESTDIR}" STREQUAL "")
    if ("${COVISE_DESTDIR}" STREQUAL "")
        getenv_path(COVISE_DESTDIR COVISEDESTDIR)
    elseif(NOT "${COVISE_DESTDIR}" STREQUAL "$ENV{COVISEDESTDIR}")
        message("COVISE_DESTDIR already set to ${COVISE_DESTDIR}, ignoring $COVISEDESTDIR=$ENV{COVISEDESTDIR}")
    endif()
else()
    if ("${COVISE_DESTDIR}" STREQUAL "")
        message("COVISE: COVISEDESTDIR not set")
    endif()
endif()

IF(WIN32)
  SET(CMAKE_MODULE_PATH "${COVISEDIR}/cmake/windows;${CMAKE_MODULE_PATH}")
ENDIF()
IF(UNIX)
  SET(CMAKE_MODULE_PATH "${COVISEDIR}/cmake/unix;${CMAKE_MODULE_PATH}")
ENDIF()
IF(APPLE)
  SET(CMAKE_MODULE_PATH "${COVISEDIR}/cmake/apple;${CMAKE_MODULE_PATH}")
ENDIF()
IF(MINGW)
  SET(CMAKE_MODULE_PATH "${COVISEDIR}/cmake/mingw;${CMAKE_MODULE_PATH}")
ENDIF(MINGW)

find_path(COVISE_OPTIONS_FILEPATH "CoviseOptions.cmake"
    PATHS
    ${COVISEDIR}/${COVISE_ARCHSUFFIX}
    DOC "COVISE - CMake library exports"
    )
if (COVISE_OPTIONS_FILEPATH AND NOT COVISE_OPTIONS_IMPORT)
    set(COVISE_OPTIONS_IMPORT "${COVISE_OPTIONS_FILEPATH}/CoviseOptions.cmake")
    include(${COVISE_OPTIONS_IMPORT})
endif()

find_path(COVISE_EXPORTS_FILEPATH "covise-exports.cmake"
    PATHS
    ${COVISEDIR}/${COVISE_ARCHSUFFIX}
    DOC "COVISE - CMake library exports"
    )
if (COVISE_EXPORTS_FILEPATH)
    if (NOT COVISE_EXPORTS_IMPORT)
        set(COVISE_EXPORTS_IMPORT "${COVISE_EXPORTS_FILEPATH}/covise-exports.cmake")
        include(${COVISE_EXPORTS_IMPORT})
    endif()
else()
    message("COVISE: CMake library exports file not found")
endif()

if(COVISE_INCLUDE_DIR)
   set(COVISE_FIND_QUIETLY TRUE)
endif()

find_path(COVISE_INCLUDE_DIR "config/CoviseConfig.h"
   PATHS
   ${COVISEDIR}/src/kernel
   DOC "COVISE - Headers"
)

MACRO(COVISE_FIND_PACKAGE package)
   SET(SAVED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})

   STRING(TOUPPER ${package} UPPER)
   STRING(TOLOWER ${package} LOWER)
   IF(MINGW)
      SET(CMAKE_PREFIX_PATH ${MINGW_SYSROOT} ${CMAKE_PREFIX_PATH})
   ENDIF()
   IF(NOT "$ENV{EXTERNLIBS}" STREQUAL "")
      getenv_path(EXTERNLIBS EXTERNLIBS)
      SET(CMAKE_PREFIX_PATH ${EXTERNLIBS}/${LOWER}/bin ${CMAKE_PREFIX_PATH})
      SET(CMAKE_PREFIX_PATH ${EXTERNLIBS} ${CMAKE_PREFIX_PATH})
      SET(CMAKE_PREFIX_PATH ${EXTERNLIBS}/${LOWER} ${CMAKE_PREFIX_PATH})
   ENDIF()
   IF(NOT "$ENV{${UPPER}_HOME}" STREQUAL "")
      getenv_path(${UPPER}_HOME ${UPPER}_HOME)
      SET(CMAKE_PREFIX_PATH ${${UPPER}_HOME} ${CMAKE_PREFIX_PATH})
   ENDIF()
   #message("looking for package ${ARGV}")
   #message("CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
   FIND_PACKAGE(${ARGV})

   SET(CMAKE_PREFIX_PATH ${SAVED_CMAKE_PREFIX_PATH})
ENDMACRO(COVISE_FIND_PACKAGE PACKAGE)

include(FindPackageHandleStandardArgs)

macro(covise_find_library module library)
   #message("covise_find_library(${module} ${library}: searching ${COVISEDIR}/${COVISE_ARCHSUFFIX}")
   if (COVISE_EXPORTS_FILEPATH)
      set(${module}_LIBRARY ${library})
   else()
      find_library(${module}_LIBRARY
         NAMES ${library}
         PATH_SUFFIXES lib
         PATHS
         ${COVISEDIR}/${COVISE_ARCHSUFFIX}
         DOC "${module} - Library"
      )
   endif()
endmacro()

set(COVISE_COMP_VARS "")
macro(covise_find_component comp)
   if (${comp} STREQUAL virvo)
       set(complib virvo)
       set(compvar VIRVO)
   elseif (${comp} STREQUAL Alg)
       set(complib coAlg)
       set(compvar ALG)
       covise_find_package(VTK COMPONENTS vtkIOLegacy vtkFiltersCore vtkCommonCore vtkImagingCore vtkCommonDataModel vtkCommonExecutionModel NO_MODULE)
       if (VTK_FOUND)
          set(ADD_LIBS ${VTK_LIBRARIES})
       endif()
   elseif (${comp} STREQUAL GPU)
       set(complib coGPU)
       set(compvar GPU)
       covise_find_package(CUDA)
       if (CUDA_FOUND)
          set(ADD_LIBS ${CUDA_LIBRARIES})
       endif()
   elseif (${comp} STREQUAL webservice)
       set(complib coWS)
       set(compvar WEBSERVICE)
   else()
       set(complib co${comp})
       string(TOUPPER "${comp}" compvar)
   endif()

   set(COVISE_COMP_VARS ${COVISE_COMP_VARS};COVISE_${compvar}_LIBRARY)

   covise_find_library(COVISE_${compvar} ${complib})
   find_package_handle_standard_args(COVISE_${compvar} DEFAULT_MSG COVISE_${compvar}_LIBRARY COVISE_INCLUDE_DIR)

   if (COVISE_${compvar}_FOUND)
      set(COVISE_${compvar}_INCLUDE_DIRS ${COVISE_INCLUDE_DIR})
      set(COVISE_${compvar}_LIBRARIES ${COVISE_${compvar}_LIBRARY} ${ADD_LIBS})
   else()
      #message("COVISE ${comp} not available.")
   endif()
endmacro()

include(Qt4-5)
find_qt()

if(COVISE_FIND_COMPONENTS)
   foreach(comp ${COVISE_FIND_COMPONENTS})
      covise_find_component(${comp})
   endforeach()
else()
   set(COVISE_FIND_COMPONENTS Util File Appl Api Core Net Do Shm Config VRBClient GRMsg GPU Alg Image)

   foreach(comp ${COVISE_FIND_COMPONENTS})
      covise_find_component(${comp})
   endforeach()

   if (COVISE_USE_VIRVO)
      covise_find_component(virvo)
   endif()

   if (COVISE_BUILD_WEBSERVICE)
      covise_find_component(webservice)
   endif()
endif()

find_package_handle_standard_args(COVISE DEFAULT_MSG
   COVISEDIR COVISE_ARCHSUFFIX COVISE_DESTDIR
   COVISE_INCLUDE_DIR
   ${COVISE_COMP_VARS}
   )
mark_as_advanced(
   COVISE_UTIL_LIBRARY COVISE_FILE_LIBRARY COVISE_APPL_LIBRARY
   COVISE_API_LIBRARY COVISE_CORE_LIBRARY COVISE_NET_LIBRARY COVISE_DO_LIBRARY
   COVISE_SHM_LIBRARY COVISE_CONFIG_LIBRARY
   COVISE_VRBCLIENT_LIBRARY COVISE_GRMSG_LIBRARY COVISE_GPU_LIBRARY
   COVISE_ALG_LIBRARY COVISE_IMAGE_LIBRARY
   COVISE_INCLUDE_DIR)

if(COVISE_FOUND)
   set(COVISE_LIBRARIES ${COVISE_UTIL_LIBRARY} ${COVISE_FILE_LIBRARY})
   set(COVISE_INCLUDE_DIRS ${COVISE_INCLUDE_DIR} ${COVISE_DESTDIR}/${COVISE_ARCHSUFFIX}/include)

   file(MAKE_DIRECTORY ${COVISE_DESTDIR}/${COVISE_ARCHSUFFIX})

   if (NOT COVISE_FIND_QUIETLY)
      message("   Building for:  ${COVISE_ARCHSUFFIX}")
      message("   Installing to: ${COVISE_DESTDIR}")
   endif()
endif()

FUNCTION(COVISE_ADD_LINK_FLAGS targetname flags)
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
  # MESSAGE("added link flags ${MY_LFLAGS} to target ${targetname}")
ENDFUNCTION(COVISE_ADD_LINK_FLAGS)



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
       IF("${mode}" STREQUAL "GENERAL")
         SET(check_install "1")
       ELSEIF("${mode}" STREQUAL "${upper_build_type_str}")
         SET(check_install "1")
       ELSE("${mode}" STREQUAL "GENERAL")
         SET(check_install "0")
       ENDIF("${mode}" STREQUAL "GENERAL")
       IF(${check_install})
         # If the library is from externlibs, pack it.
         # FIXME: Currently only works with libraries added by FindXXX, as manually added
         # libraries usually lack the full path.
         string(REPLACE "++" "\\+\\+" extlibs $ENV{EXTERNLIBS})
         IF (value MATCHES "^${extlibs}")
#           MESSAGE("VALUE+ ${value}")
           IF (IS_DIRECTORY ${value})
           ELSE (IS_DIRECTORY ${value})
              INSTALL(FILES ${value} DESTINATION covise/${COVISE_ARCHSUFFIX}/lib)
           # Recurse symlinks
           # FIXME: Does not work with absolute links (that are evil anyway)
           WHILE(IS_SYMLINK ${value})
             EXECUTE_PROCESS(COMMAND readlink -n ${value} OUTPUT_VARIABLE link_target)
             GET_FILENAME_COMPONENT(link_dir ${value} PATH)
             SET(value "${link_dir}/${link_target}")
#             MESSAGE("VALUE ${value}")
INSTALL(FILES ${value} DESTINATION covise/${COVISE_ARCHSUFFIX}/lib)
           ENDWHILE(IS_SYMLINK ${value})
           ENDIF (IS_DIRECTORY ${value})
         ENDIF (value MATCHES "^${extlibs}")
       ENDIF(${check_install})
     ENDFOREACH(ctr)
  ENDIF (${targetname}_LIB_DEPENDS)
ENDMACRO(COVISE_INSTALL_DEPENDENCIES)




# Macro to install and export
MACRO(COVISE_INSTALL_TARGET targetname)
  # was a category specified for the given target?
  GET_TARGET_PROPERTY(category ${targetname} LABELS)
  SET(_category_path )
  IF(category)
    SET(_category_path "/${category}")
  ENDIF(category)

  # @Florian: What are you trying to do? The following will create a
  # subdirectory "covise/${COVISE_ARCHSUFFIX}/..."
  # in each and every in-source subdirectory where you issue a COVISE_INSTALL_TARGET() !!!
  # cmake's INSTALL() will create the given subdirectories in ${CMAKE_INSTALL_PREFIX} at install time.
  #
  #  IF (NOT EXISTS covise/${COVISE_ARCHSUFFIX}/bin)
  #    FILE(MAKE_DIRECTORY covise/${COVISE_ARCHSUFFIX}/bin)
  #  ENDIF(NOT EXISTS covise/${COVISE_ARCHSUFFIX}/bin)
  #  IF (NOT EXISTS covise/${COVISE_ARCHSUFFIX}/bin${_category_path})
  #    FILE(MAKE_DIRECTORY covise/${COVISE_ARCHSUFFIX}/bin${_category_path})
  #  ENDIF(NOT EXISTS covise/${COVISE_ARCHSUFFIX}/bin${_category_path})
  #  IF (NOT EXISTS covise/${COVISE_ARCHSUFFIX}/lib)
  #    FILE(MAKE_DIRECTORY covise/${COVISE_ARCHSUFFIX}/lib)
  #  ENDIF(NOT EXISTS covise/${COVISE_ARCHSUFFIX}/lib)

  INSTALL(TARGETS ${ARGV} EXPORT covise-targets
     RUNTIME DESTINATION covise/${COVISE_ARCHSUFFIX}/bin${_category_path}
     LIBRARY DESTINATION covise/${COVISE_ARCHSUFFIX}/lib
     ARCHIVE DESTINATION covise/${COVISE_ARCHSUFFIX}/lib
          COMPONENT modules.${category}
  )
  IF(COVISE_EXPORT_TO_INSTALL)
     INSTALL(EXPORT covise-targets DESTINATION covise/${COVISE_ARCHSUFFIX}/lib COMPONENT modules.${category})
  ELSE(COVISE_EXPORT_TO_INSTALL)
    # EXPORT(TARGETS ${ARGV} APPEND FILE # "${CMAKE_BINARY_DIR}/${BASEARCHSUFFIX}/${COVISE_EXPORT_FILE}")
    EXPORT(TARGETS ${ARGV} APPEND FILE "${COVISE_DESTDIR}/${COVISE_ARCHSUFFIX}/${COVISE_EXPORT_FILE}")
  ENDIF(COVISE_EXPORT_TO_INSTALL)
  FOREACH(tgt ${ARGV})
     COVISE_COPY_TARGET_PDB(${tgt} covise/${COVISE_ARCHSUFFIX} ${_category_path})
  ENDFOREACH(tgt)
  COVISE_INSTALL_DEPENDENCIES(${targetname})
ENDMACRO(COVISE_INSTALL_TARGET)

# Macro to install an OpenCOVER plugin
MACRO(COVER_INSTALL_PLUGIN targetname)
  INSTALL(TARGETS ${ARGV} EXPORT covise-targets
     RUNTIME DESTINATION covise/${COVISE_ARCHSUFFIX}/lib/OpenCOVER/plugins
     LIBRARY DESTINATION covise/${COVISE_ARCHSUFFIX}/lib/OpenCOVER/plugins
     ARCHIVE DESTINATION covise/${COVISE_ARCHSUFFIX}/lib/OpenCOVER/plugins
          COMPONENT osgplugins.${category}
  )
  COVISE_INSTALL_DEPENDENCIES(${targetname})
ENDMACRO(COVER_INSTALL_PLUGIN)


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
        SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${COVISE_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${COVISE_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_build_type_str} "${COVISE_DESTDIR}/${COVISE_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
      ELSE(COVISE_DESTDIR)
        SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${COVISE_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${COVISE_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
        SET_TARGET_PROPERTIES(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper_build_type_str} "${CMAKE_BINARY_DIR}/${COVISE_ARCHSUFFIX}/${BINLIB_SUFFIX}${MYPATH_POSTFIX}")
      ENDIF(COVISE_DESTDIR)
    ENDIF(CMAKE_CONFIGURATION_TYPES)

  ELSE(TARGET_TYPE)
    MESSAGE("COVISE_ADJUST_OUTPUT_DIR() - Could not retrieve target type of ${targetname}")
  ENDIF(TARGET_TYPE)
ENDFUNCTION(COVISE_ADJUST_OUTPUT_DIR)

# Macro to add covise modules (executables with a module-category)
MACRO(covise_add_module category targetname)
  ADD_EXECUTABLE(${targetname} ${ARGN})
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

  target_link_libraries(${targetname}
     ${COVISE_DO_LIBRARY}
     ${COVISE_SHM_LIBRARY}
     ${COVISE_UTIL_LIBRARY}
     ${COVISE_API_LIBRARY}
     ${COVISE_APPL_LIBRARY}
     ${COVISE_CORE_LIBRARY}
     ${COVISE_CONFIG_LIBRARY}
     ${COVISE_ALG_LIBRARY}
  )

  covise_install_target(${targetname})

  INCLUDE_DIRECTORIES(
     ${COVISE_INCLUDE_DIRS}
  )
ENDMACRO(covise_add_module)

MACRO(USING_MESSAGE)
   #MESSAGE(${ARGN})
ENDMACRO(USING_MESSAGE)

MACRO(covise_create_using)
  getenv_path(EXTERNLIBS EXTERNLIBS)
  FIND_PROGRAM(GREP_EXECUTABLE grep PATHS ${EXTERNLIBS}/UnixUtils DOC "grep executable")

  file(GLOB USING_FILES "${COVISEDIR}/cmake/Using/Use*.cmake")
  if (USING_FILES)
     foreach(F ${USING_FILES})
        #message("Using: including ${F}")
        include(${F})
     endforeach(F ${USING_FILES})
     EXECUTE_PROCESS(COMMAND ${GREP_EXECUTABLE} -h USE_ ${USING_FILES}
        COMMAND ${GREP_EXECUTABLE} "^MACRO"
        OUTPUT_VARIABLE using_list)
     #message("using_list")
     #message("using_list ${using_list}")
  endif()

  STRING(STRIP "${using_list}" using_list)

  STRING(REGEX REPLACE "MACRO\\([^_]*_\([^\n]*\)\\)" "\\1" using_list "${using_list}")
  STRING(REGEX REPLACE " optional" "" using_list "${using_list}")
  STRING(REGEX REPLACE "\n" ";" using_list "${using_list}")

  #MESSAGE("USING list: ${using_list}")

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

ENDMACRO(covise_create_using)

# Create macro list for USING
#covise_create_using()

# Macro to add covise libraries
MACRO(COVISE_ADD_LIBRARY targetname)
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
ENDMACRO(COVISE_ADD_LIBRARY)

