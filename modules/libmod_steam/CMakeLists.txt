#FIXME: This was not enough
#if($ENV{STEAMWORKSDIR})
	project(mod_steam)

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")

	set(CMAKE_MODULE_PATH ../../cmake/Modules)

	set(CMAKE_CXX_FLAGS "-fpermissive")

	find_package(STEAMWORKS REQUIRED)

	add_definitions(${EXTRA_CFLAGS})

	include_directories(${STEAMWORKS_INCLUDE_DIR} ../../core/include ../../core/bgdrtm ../../modules/libmod_steam)

	file(GLOB SOURCES_LIBMOD_STEAM
		 "../../modules/libmod_steam/*.cpp"
		 )

	add_library(mod_steam ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_STEAM})
	target_link_libraries(mod_steam ${STEAMWORKS_LIBRARIES} -L../../bin bgdrtm)
#endif()