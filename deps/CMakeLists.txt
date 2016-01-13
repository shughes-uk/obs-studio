
if(NOT MINGW)
add_subdirectory(w32-pthreads)
endif()

add_subdirectory(glad)
add_subdirectory(ipc-util)
add_subdirectory(libff)
add_subdirectory(file-updater)

find_package(Jansson 2.5 QUIET)
find_package(libwebsockets QUIET)
if (NOT LIBWEBSOCKETS_FOUND)
	message(STATUS "LIBWEBSOCKETS >=2.5 not found, building bundled version")

	add_subdirectory(libwebsockets)

	set(OBS_LIBWEBSOCKETS_IMPORT "websockets" CACHE INTERNAL "Internal var")
	set(OBS_LIBWEBSOCKETS_INCLUDE_DIR "" CACHE INTERNAL "Internal var")
	message(STATUS "Using system libwebsockets library ${LIBWEBSOCKETS_LIBRARIES_STATIC}")

else()
	message(STATUS "Using system libwebsockets library")
	set(OBS_LIBWEBSOCKETS_IMPORT "${LIBWEBSOCKETS_LIBRARIES}" CACHE INTERNAL "Internal var")
	set(OBS_LIBWEBSOCKETS_INCLUDE_DIR "${LIBWEBSOCKETS_INCLUDE_DIR}" CACHE INTERNAL "Internal var")
endif()

if(NOT JANSSON_FOUND)
	message(STATUS "Jansson >=2.5 not found, building bundled version")

	add_subdirectory(jansson)

	set(OBS_JANSSON_IMPORT "jansson" CACHE INTERNAL "Internal var")
	set(OBS_JANSSON_INCLUDE_DIRS "" CACHE INTERNAL "Internal var")
else()
	message(STATUS "Using system Jansson library")

	set(OBS_JANSSON_IMPORT "${JANSSON_LIBRARIES}" CACHE INTERNAL "Internal var")
	set(OBS_JANSSON_INCLUDE_DIRS "${JANSSON_INCLUDE_DIRS}" CACHE INTERNAL "Internal var")
endif()
