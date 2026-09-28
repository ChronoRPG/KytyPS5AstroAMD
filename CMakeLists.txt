cmake_minimum_required(VERSION 3.16...4.3)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for bundled dependencies")
endif()

get_property(KYTY_GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT KYTY_GENERATOR_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

project(Kyty)

find_program(SCCACHE_PROGRAM sccache)
find_program(CCACHE_PROGRAM ccache)

foreach(lang IN ITEMS C CXX)
    # An explicitly empty launcher disables caching for this language.
    if(NOT DEFINED CMAKE_${lang}_COMPILER_LAUNCHER)
        if(SCCACHE_PROGRAM)
            set(CMAKE_${lang}_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
            message(STATUS "sccache enabled for ${lang}: ${SCCACHE_PROGRAM}")
        elseif(CCACHE_PROGRAM)
            set(CMAKE_${lang}_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
            message(STATUS "ccache enabled for ${lang}: ${CCACHE_PROGRAM}")
        endif()
    endif()
endforeach()

set(KYTY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(KYTY_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(KYTY_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty")

if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
	set(LINUX TRUE)
endif()

if (NOT (WIN32 OR LINUX OR APPLE))
    message(FATAL_ERROR "only Windows, Linux, and macOS builds are supported")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include("${KYTY_SOURCE_DIR}/utils.cmake")
include(CTest)

option(KYTY_ENABLE_CLANG_TIDY "Run clang-tidy checks during builds" OFF)

set(KYTY_BUILD_ORIGIN "Source" CACHE STRING "Build provenance shown in diagnostics")
set_property(CACHE KYTY_BUILD_ORIGIN PROPERTY STRINGS Source Official Fork)
if(NOT KYTY_BUILD_ORIGIN STREQUAL "Source" AND NOT KYTY_BUILD_ORIGIN STREQUAL "Official" AND
   NOT KYTY_BUILD_ORIGIN STREQUAL "Fork")
	message(FATAL_ERROR "KYTY_BUILD_ORIGIN must be Source, Official, or Fork")
endif()

set(KYTY_RELEASE_TAG "" CACHE STRING "Release tag embedded in official builds")
set(KYTY_BUILD_REPOSITORY "" CACHE STRING "GitHub repository embedded in fork builds")
set(KYTY_OFFICIAL_BUILD OFF)
set(KYTY_FORK_BUILD OFF)
if(KYTY_BUILD_ORIGIN STREQUAL "Official")
	if(KYTY_RELEASE_TAG STREQUAL "")
		message(FATAL_ERROR "Official builds require KYTY_RELEASE_TAG")
	endif()
	set(KYTY_OFFICIAL_BUILD ON)
elseif(KYTY_BUILD_ORIGIN STREQUAL "Fork")
	if(KYTY_BUILD_REPOSITORY STREQUAL "")
		message(FATAL_ERROR "Fork builds require KYTY_BUILD_REPOSITORY")
	endif()
	set(KYTY_FORK_BUILD ON)
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(TestBigEndian)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "only 64-bit builds are supported")
endif()

test_big_endian(BIG_ENDIAN)
if(BIG_ENDIAN)
	set(KYTY_ENDIAN KYTY_ENDIAN_BIG)
else()
	set(KYTY_ENDIAN KYTY_ENDIAN_LITTLE)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(KYTY_BUILD KYTY_BUILD_DEBUG)
else()
	set(KYTY_BUILD KYTY_BUILD_RELEASE)
endif()

if(LINUX OR APPLE)
	# macOS rides the POSIX/Linux code paths until it gets a dedicated platform
	set(KYTY_PLATFORM KYTY_PLATFORM_LINUX)
else()
	set(KYTY_PLATFORM KYTY_PLATFORM_WINDOWS)
endif()

set(CLANG FALSE)
set(GCC FALSE)
set(KYTY_CLANG_CL FALSE)
get_filename_component(KYTY_CXX_COMPILER_NAME "${CMAKE_CXX_COMPILER}" NAME_WE)
string(TOLOWER "${KYTY_CXX_COMPILER_NAME}" KYTY_CXX_COMPILER_NAME)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
	set(CLANG TRUE)
	set(KYTY_COMPILER CLANG)
	if(WIN32 AND KYTY_CXX_COMPILER_NAME STREQUAL "clang-cl")
		set(KYTY_CLANG_CL TRUE)
		set(KYTY_LINKER LLD_LINK)
	elseif(APPLE)
		set(KYTY_LINKER LD64) # Apple's default linker; lld flags don't apply
	else()
		set(KYTY_LINKER LLD)
	endif()
	if ((CMAKE_AR MATCHES "llvm") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0.0) AND (CMAKE_CXX_COMPILER MATCHES "winlibs"))
		string(REPLACE "llvm-ar.exe" "ar.exe" CMAKE_AR ${CMAKE_AR}) # llvm-ar.exe 11.0.0 from winlibs is broken
	endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(GCC TRUE)
	set(KYTY_COMPILER GCC)
	set(KYTY_LINKER LD)
else()
	message(FATAL_ERROR "only Clang and GCC are supported; found ${CMAKE_CXX_COMPILER_ID}")
endif()

set(KYTY_PROJECT_NAME "Emulator" CACHE STRING "Project name suffix")

string(TOLOWER ${KYTY_COMPILER} KYTY_COMPILER_ID)
string(TOLOWER ${KYTY_LINKER} KYTY_LINKER_ID)

if (CLANG AND (KYTY_LINKER STREQUAL LLD))
	set(KYTY_LD_OPTIONS "-fuse-ld=lld")
endif()

if (KYTY_LINKER STREQUAL LD AND NOT LINUX)
	set(KYTY_LD_OPTIONS "-Wl,--image-base=0x100000000000")
endif()

project(Kyty${KYTY_PROJECT_NAME}${CMAKE_BUILD_TYPE}${KYTY_COMPILER} VERSION 0.3.0)

include_directories(
	${KYTY_THIRD_PARTY_DIR}/gtest/include
	${KYTY_THIRD_PARTY_DIR}/gtest
	${KYTY_THIRD_PARTY_DIR}/fmt/include
	${KYTY_THIRD_PARTY_DIR}/magic_enum/include/magic_enum
	${PROJECT_BINARY_DIR}
	${KYTY_SOURCE_DIR}
)

set(KYTY_VERSION "${PROJECT_VERSION}")

configure_file(
  ${KYTY_SOURCE_DIR}/cmake_config.h.in
  ${PROJECT_BINARY_DIR}/cmake_config.h
  )

find_package(Git)

add_custom_target(KytyGitVersion
	COMMAND ${CMAKE_COMMAND}
		-D INPUT_FILE=${KYTY_SOURCE_DIR}/kytyGitVersion.h.in
		-D OUTPUT_FILE=${PROJECT_BINARY_DIR}/kytyGitVersion.h
		-D GIT_EXECUTABLE=${GIT_EXECUTABLE}
		-D GIT_WORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${KYTY_SOURCE_DIR}/generate_version.cmake
	BYPRODUCTS ${PROJECT_BINARY_DIR}/kytyGitVersion.h
	COMMENT "Generate kytyGitVersion.h"
)

add_library(kyty_git_version INTERFACE)
add_dependencies(kyty_git_version KytyGitVersion)

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0.0)
	list(APPEND KYTY_IWYU
		kyty_emulator
		common
		#launcher
	)
	list(APPEND KYTY_CLANG_TIDY
		kyty_emulator
		#common
		launcher
	)
endif()

option(KYTY_BUILD_LAUNCHER "Build Qt launcher" ON)

config_compiler_and_linker()

add_subdirectory("${KYTY_THIRD_PARTY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/3rdparty")
add_subdirectory("${KYTY_SOURCE_DIR}/common" "${CMAKE_CURRENT_BINARY_DIR}/common")

file(GLOB kyty_emulator_src CONFIGURE_DEPENDS
	"${KYTY_SOURCE_DIR}/libs/*.cpp"
	"${KYTY_SOURCE_DIR}/libs/*.h"
	"${KYTY_SOURCE_DIR}/graphics/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/*.h"
	"${KYTY_SOURCE_DIR}/graphics/guest_gpu/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/guest_gpu/*.h"
	"${KYTY_SOURCE_DIR}/graphics/guest_gpu/command_processor/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/guest_gpu/command_processor/*.h"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/*.h"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/*.h"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/cache/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/cache/*.h"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/drawPrep/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/drawPrep/*.h"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/image/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/image/*.h"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/pipeline/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/pipeline/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/frontend/cfg/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/frontend/cfg/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/frontend/decode/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/frontend/decode/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/frontend/translate/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/frontend/translate/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/backend/spirv/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/backend/spirv/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/opcodes/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/opcodes/*.h"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/opcodes/*.inc"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/*.h"
	"${KYTY_SOURCE_DIR}/graphics/presentation/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/presentation/*.h"
	"${KYTY_SOURCE_DIR}/graphics/presentation/window/*.cpp"
	"${KYTY_SOURCE_DIR}/graphics/presentation/window/*.h"
	"${KYTY_SOURCE_DIR}/kernel/*.cpp"
	"${KYTY_SOURCE_DIR}/kernel/*.h"
	"${KYTY_SOURCE_DIR}/loader/*.cpp"
	"${KYTY_SOURCE_DIR}/loader/*.h"
)

find_program(KYTY_GLSLANG_VALIDATOR glslangValidator REQUIRED)
set(gpu_tiler_shader_dir "${KYTY_SOURCE_DIR}/graphics/host_gpu/shaders")
set(gpu_tiler_generated_dir "${PROJECT_BINARY_DIR}/gpu_tiler_shaders")
set(gpu_tiler_shader_names
	standard256
	standard4
	standard4_3d
	standard64
	standard64_3d
	prt
	prt_3d
	render_target
	depth
	promote_d16
	demote_d16
	swap_bgra16
)
file(GLOB gpu_tiler_shader_includes CONFIGURE_DEPENDS "${gpu_tiler_shader_dir}/gpu_tiler_*.inc")
foreach(shader_name IN LISTS gpu_tiler_shader_names)
	set(shader_source "${gpu_tiler_shader_dir}/gpu_tiler_${shader_name}.comp")
	set(shader_spv "${gpu_tiler_generated_dir}/gpu_tiler_${shader_name}.spv")
	set(shader_header "${gpu_tiler_generated_dir}/gpu_tiler_${shader_name}_spv.h")
	string(TOUPPER "GPU_TILER_${shader_name}_SPV" shader_symbol)
	add_custom_command(
		OUTPUT "${shader_header}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_tiler_generated_dir}"
		COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -Os
		        "-I${gpu_tiler_shader_dir}" -o "${shader_spv}" "${shader_source}"
		COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
		        -DSYMBOL=${shader_symbol} -P "${KYTY_SOURCE_DIR}/embed_spirv.cmake"
		DEPENDS "${shader_source}" ${gpu_tiler_shader_includes}
		VERBATIM
	)
	list(APPEND gpu_tiler_shader_headers "${shader_header}")
endforeach()
# Image variants of the 2D tile families (TileManager::DetileToImage / TileFromImage): the same
# sources with the linear side in a native image instead of a buffer.
set(gpu_tiler_image_family_names
	standard256
	standard4
	standard64
	prt
	render_target
	depth
)
foreach(shader_name IN LISTS gpu_tiler_image_family_names)
	foreach(direction IN ITEMS store load)
		string(TOUPPER "${direction}" direction_upper)
		set(shader_source "${gpu_tiler_shader_dir}/gpu_tiler_${shader_name}.comp")
		set(shader_spv "${gpu_tiler_generated_dir}/gpu_tiler_image_${direction}_${shader_name}.spv")
		set(shader_header
		    "${gpu_tiler_generated_dir}/gpu_tiler_image_${direction}_${shader_name}_spv.h")
		string(TOUPPER "GPU_TILER_IMAGE_${direction}_${shader_name}_SPV" shader_symbol)
		add_custom_command(
			OUTPUT "${shader_header}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_tiler_generated_dir}"
			COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -Os
			        "-DTILER_IMAGE_${direction_upper}" "-I${gpu_tiler_shader_dir}"
			        -o "${shader_spv}" "${shader_source}"
			COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
			        -DSYMBOL=${shader_symbol} -P "${KYTY_SOURCE_DIR}/embed_spirv.cmake"
			DEPENDS "${shader_source}" ${gpu_tiler_shader_includes}
			VERBATIM
		)
		list(APPEND gpu_tiler_shader_headers "${shader_header}")
	endforeach()
endforeach()

set(fault_buffer_shader_source "${gpu_tiler_shader_dir}/fault_buffer_process.comp")
set(fault_buffer_shader_spv "${gpu_tiler_generated_dir}/fault_buffer_process.spv")
set(fault_buffer_shader_header "${gpu_tiler_generated_dir}/fault_buffer_process_spv.h")
add_custom_command(
	OUTPUT "${fault_buffer_shader_header}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_tiler_generated_dir}"
	COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -Os
	        -DCACHING_PAGEBITS=14 -DMAX_PAGE_FAULTS=1024
	        -o "${fault_buffer_shader_spv}" "${fault_buffer_shader_source}"
	COMMAND ${CMAKE_COMMAND} -DINPUT=${fault_buffer_shader_spv} -DOUTPUT=${fault_buffer_shader_header}
	        -DSYMBOL=FAULT_BUFFER_PROCESS_SPV -P "${KYTY_SOURCE_DIR}/embed_spirv.cmake"
	DEPENDS "${fault_buffer_shader_source}"
	VERBATIM
)
list(APPEND kyty_emulator_src "${fault_buffer_shader_header}")
list(APPEND kyty_emulator_src ${gpu_tiler_shader_headers})

set(gpu_blit_generated_dir "${PROJECT_BINARY_DIR}/gpu_blit_shaders")
set(gpu_blit_shader_sources
	"${gpu_tiler_shader_dir}/gpu_blit_fs_triangle.vert"
	"${gpu_tiler_shader_dir}/gpu_blit_color_to_ms_depth.frag"
	"${gpu_tiler_shader_dir}/gpu_blit_depth_to_color32.comp"
	"${gpu_tiler_shader_dir}/gpu_blit_color32_to_depth.frag"
)
foreach(shader_source IN LISTS gpu_blit_shader_sources)
	get_filename_component(shader_name "${shader_source}" NAME_WE)
	set(shader_spv "${gpu_blit_generated_dir}/${shader_name}.spv")
	set(shader_header "${gpu_blit_generated_dir}/${shader_name}_spv.h")
	string(TOUPPER "${shader_name}_SPV" shader_symbol)
	add_custom_command(
		OUTPUT "${shader_header}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_blit_generated_dir}"
		COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -Os
		        "-I${gpu_tiler_shader_dir}" -o "${shader_spv}" "${shader_source}"
		COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
		        -DSYMBOL=${shader_symbol} -P "${KYTY_SOURCE_DIR}/embed_spirv.cmake"
		DEPENDS "${shader_source}"
		VERBATIM
	)
	list(APPEND gpu_blit_shader_headers "${shader_header}")
endforeach()
list(APPEND kyty_emulator_src ${gpu_blit_shader_headers})

set(gpu_dcc_generated_dir "${PROJECT_BINARY_DIR}/gpu_dcc_shaders")
set(gpu_dcc_shader_names validate clear occlusion)
foreach(shader_name IN LISTS gpu_dcc_shader_names)
	set(shader_source "${gpu_tiler_shader_dir}/gpu_dcc_${shader_name}.comp")
	set(shader_spv "${gpu_dcc_generated_dir}/gpu_dcc_${shader_name}.spv")
	set(shader_header "${gpu_dcc_generated_dir}/gpu_dcc_${shader_name}_spv.h")
	string(TOUPPER "GPU_DCC_${shader_name}_SPV" shader_symbol)
	add_custom_command(
		OUTPUT "${shader_header}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_dcc_generated_dir}"
		COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -Os
		        -o "${shader_spv}" "${shader_source}"
		COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
		        -DSYMBOL=${shader_symbol} -P "${KYTY_SOURCE_DIR}/embed_spirv.cmake"
		DEPENDS "${shader_source}"
		VERBATIM
	)
	list(APPEND kyty_emulator_src "${shader_header}")
endforeach()

list(APPEND kyty_emulator_src
	"${KYTY_SOURCE_DIR}/emulator.h"
	"${KYTY_SOURCE_DIR}/emulator.cpp"
)
list(REMOVE_DUPLICATES kyty_emulator_src)

if (KYTY_CLANG_CL)
	set_source_files_properties(${kyty_emulator_src} PROPERTIES COMPILE_FLAGS "-Wno-pragma-pack -Wno-deprecated-declarations -D_TIMESPEC_DEFINED")
endif()

set(kyty_emulator_link_libraries common Vulkan::Headers spirv-tools-opt spirv-tools SDL3::SDL3-static imgui xxhash FFmpeg::ffmpeg fmt::fmt nlohmann_json::nlohmann_json LibAtrac9 kyty_git_version Zydis::Zydis xbyak::xbyak)

# Linux system libraries.
if(LINUX)
	find_package(Threads REQUIRED)
	list(APPEND kyty_emulator_link_libraries m ${CMAKE_DL_LIBS} Threads::Threads)
endif()

set(inc_headers
	${KYTY_SOURCE_DIR}
	${KYTY_THIRD_PARTY_DIR}/VulkanMemoryAllocator/include
	${KYTY_THIRD_PARTY_DIR}/SPIRV-Tools/include
	${KYTY_THIRD_PARTY_DIR}/SPIRV-Headers/include
	${KYTY_THIRD_PARTY_DIR}/fmt/include
	${KYTY_THIRD_PARTY_DIR}/xxhash
	${KYTY_THIRD_PARTY_DIR}/cpuinfo/include
	${KYTY_THIRD_PARTY_DIR}/renderdoc
	${KYTY_THIRD_PARTY_DIR}/stb
)

if (KYTY_CLANG_CL)
	list(APPEND kyty_emulator_link_libraries winpthread)
	list(APPEND inc_headers
		${KYTY_THIRD_PARTY_DIR}/winpthread/include
	)
endif()

list(APPEND check_headers
	${KYTY_SOURCE_DIR}
)

function(add_kyty_full_emulator_test target source)
	add_executable(${target} EXCLUDE_FROM_ALL ${source} ${kyty_emulator_src})
	target_link_libraries(${target} ${kyty_emulator_link_libraries})
	target_include_directories(${target} PRIVATE ${inc_headers})
	if (WIN32)
		target_link_libraries(${target} iphlpapi)
	endif()
	if (KYTY_CLANG_CL)
		target_link_libraries(${target} onecore)
		add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" $<TARGET_FILE_DIR:${target}>/libwinpthread-1.dll)
	endif()
	# The macOS x86_64 guest address space needs its .zerofill segments anchored
	# by linker flags, or the kernel kills the binary on load (posix_spawn EIO).
	configure_macos_guest_address_space(${target})
endfunction()

function(configure_macos_guest_address_space target)
	if(APPLE AND (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR
			(NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")))
		target_sources(${target} PRIVATE "${KYTY_SOURCE_DIR}/kernel/macosGuestAddressSpace.cpp")
		target_compile_definitions(${target} PRIVATE KYTY_LINKED_GUEST_ADDRESS_SPACE=1)
		target_link_options(${target} PRIVATE
			-Wl,-ld_classic,-no_pie,-no_fixup_chains,-no_huge,-pagezero_size,0x40000,-segaddr,SYSTEM_MANAGED,0x40000,-segaddr,SYSTEM_RESERVED,0x7ffffc000,-segaddr,USER_AREA,0x7000000000,-image_base,0x700000000000)
	endif()
endfunction()

add_kyty_full_emulator_test(shader_cfg_tests "${KYTY_TESTS_DIR}/shaderCfgTests.cpp")

add_executable(scalar_provenance_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ScalarProvenanceTests.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/CodegenOptions.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/hostMemory.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/ReadLaneElimination.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/SrtWalker.cpp"
)
target_link_libraries(scalar_provenance_tests common fmt::fmt)
target_include_directories(scalar_provenance_tests PRIVATE ${inc_headers})

add_executable(page_manager_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/PageManagerTests.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/pageManager.cpp"
)
target_link_libraries(page_manager_tests common)
target_include_directories(page_manager_tests PRIVATE ${inc_headers})

add_executable(bit_array_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/BitArrayTests.cpp"
)
target_include_directories(bit_array_tests PRIVATE ${inc_headers})

# Per-thread profiler counters (common/profiler.h); also prints a timing of the counting paths.
add_executable(profiler_counter_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ProfilerCounterTests.cpp"
)
target_link_libraries(profiler_counter_tests common)
target_include_directories(profiler_counter_tests PRIVATE ${inc_headers})
if(LINUX)
	target_link_libraries(profiler_counter_tests Threads::Threads)
endif()

add_executable(lod_stats_report_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/LodStatsReportTests.cpp"
)
target_include_directories(lod_stats_report_tests PRIVATE ${inc_headers})

add_executable(ime_dialog_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ImeDialogTests.cpp"
	"${KYTY_SOURCE_DIR}/libs/imeCommon.cpp"
	"${KYTY_SOURCE_DIR}/libs/imeDialog.cpp"
)
target_include_directories(ime_dialog_tests PRIVATE ${inc_headers})

add_executable(memory_tracker_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/MemoryTrackerTests.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/pageManager.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/memoryTracker.cpp"
)
target_link_libraries(memory_tracker_tests fmt::fmt common)
target_include_directories(memory_tracker_tests PRIVATE ${inc_headers})

add_executable(shader_vertex_metadata_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ShaderVertexMetadataTests.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/hostMemory.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/shaderVertexMetadata.cpp"
)
target_include_directories(shader_vertex_metadata_tests PRIVATE ${inc_headers})

add_executable(resource_materialization_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ResourceMaterializationTests.cpp"
	"${KYTY_SOURCE_DIR}/graphics/guest_gpu/gpu_format.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/hostMemory.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/SrtWalker.cpp"
)
target_link_libraries(resource_materialization_tests common fmt::fmt xxhash)
if(LINUX)
	# The shared-plan evaluation case runs std::thread workers.
	target_link_libraries(resource_materialization_tests Threads::Threads)
endif()
target_include_directories(resource_materialization_tests PRIVATE ${inc_headers})

add_executable(resource_tracking_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ResourceTrackingTests.cpp"
	"${KYTY_SOURCE_DIR}/graphics/guest_gpu/gpu_format.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/hostMemory.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/shaderPixelParameter.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/SrtWalker.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/ResourceTracking.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/ShaderInfoCollection.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/BindingLayout.cpp"
	"${KYTY_SOURCE_DIR}/graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.cpp"
)
target_link_libraries(resource_tracking_tests common fmt::fmt)
target_include_directories(resource_tracking_tests PRIVATE ${inc_headers})

add_executable(audio_out2_port_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/AudioOut2PortTests.cpp"
	"${KYTY_SOURCE_DIR}/libs/libAudio2.cpp"
	"${KYTY_SOURCE_DIR}/loader/timer.cpp"
)
target_link_libraries(audio_out2_port_tests common fmt::fmt)
target_include_directories(audio_out2_port_tests PRIVATE ${inc_headers})

add_executable(save_data_memory_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/SaveDataMemoryTests.cpp"
)
target_link_libraries(save_data_memory_tests common fmt::fmt)
target_include_directories(save_data_memory_tests PRIVATE ${inc_headers})

add_executable(event_queue_lifetime_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/EventQueueLifetimeTests.cpp"
	"${KYTY_SOURCE_DIR}/kernel/eventQueue.cpp"
	"${KYTY_SOURCE_DIR}/loader/timer.cpp"
)
target_link_libraries(event_queue_lifetime_tests common fmt::fmt)
target_include_directories(event_queue_lifetime_tests PRIVATE ${inc_headers})

add_kyty_full_emulator_test(kernel_file_system_tests
	"${KYTY_TESTS_DIR}/KernelFileSystemTests.cpp")

add_executable(sync_on_address_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/SyncOnAddressTests.cpp"
	"${KYTY_SOURCE_DIR}/kernel/syncOnAddress.cpp"
)
target_link_libraries(sync_on_address_tests common)
target_include_directories(sync_on_address_tests PRIVATE ${inc_headers})

add_executable(image_page_table_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ImagePageTableTests.cpp"
)
target_link_libraries(image_page_table_tests common fmt::fmt)
target_include_directories(image_page_table_tests PRIVATE ${inc_headers})

add_executable(image_cache_gc_policy_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/ImageCacheGcPolicyTests.cpp"
)
target_include_directories(image_cache_gc_policy_tests PRIVATE "${KYTY_SOURCE_DIR}")

add_executable(draw_prep_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/DrawPrepTests.cpp"
)
target_include_directories(draw_prep_tests PRIVATE ${inc_headers})
# ReadSet digest certificates hash with XXH3.
target_link_libraries(draw_prep_tests xxhash)
if(LINUX)
	target_link_libraries(draw_prep_tests Threads::Threads)
endif()

add_executable(ampr_counter_bank_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/AmprCounterBankTests.cpp"
)
target_include_directories(ampr_counter_bank_tests PRIVATE "${KYTY_SOURCE_DIR}")

# Pure helpers of the texture/descriptor binding path; no Vulkan device is created.
add_executable(binding_path_tests EXCLUDE_FROM_ALL
	"${KYTY_TESTS_DIR}/BindingPathTests.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.cpp"
	"${KYTY_SOURCE_DIR}/graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.cpp"
)
target_link_libraries(binding_path_tests common fmt::fmt xxhash Vulkan::Headers)
target_include_directories(binding_path_tests PRIVATE ${inc_headers})

add_kyty_full_emulator_test(shader_recompiler_compute_tests "${KYTY_TESTS_DIR}/ShaderRecompilerComputeTests.cpp")

set(gpu_test_generated_dir "${PROJECT_BINARY_DIR}/gpu_test_shaders")
set(gpu_test_ms_depth_source
	"${gpu_tiler_shader_dir}/gpu_test_ms_depth.comp")
set(gpu_test_ms_depth_spv
	"${gpu_test_generated_dir}/gpu_test_ms_depth.spv")
set(gpu_test_ms_depth_header
	"${gpu_test_generated_dir}/gpu_test_ms_depth_spv.h")
add_custom_command(
	OUTPUT "${gpu_test_ms_depth_header}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_test_generated_dir}"
	COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -Os
	        -o "${gpu_test_ms_depth_spv}" "${gpu_test_ms_depth_source}"
	COMMAND ${CMAKE_COMMAND} -DINPUT=${gpu_test_ms_depth_spv}
	        -DOUTPUT=${gpu_test_ms_depth_header} -DSYMBOL=GPU_TEST_MS_DEPTH_SPV
	        -P "${KYTY_SOURCE_DIR}/embed_spirv.cmake"
	DEPENDS "${gpu_test_ms_depth_source}"
	VERBATIM
)
target_sources(shader_recompiler_compute_tests PRIVATE
	"${gpu_test_ms_depth_header}")

add_kyty_full_emulator_test(virtual_memory_allocation_tests "${KYTY_TESTS_DIR}/VirtualMemoryAllocationTests.cpp")
add_kyty_full_emulator_test(repeat_trace_tests "${KYTY_TESTS_DIR}/RepeatTraceTests.cpp")
target_compile_definitions(virtual_memory_allocation_tests PRIVATE
	KYTY_VIRTUAL_MEMORY_ALLOCATION_TESTS=1)

# These tests use exceptions.
if(NOT KYTY_CLANG_CL)
	foreach(kyty_exception_test scalar_provenance_tests resource_tracking_tests
			virtual_memory_allocation_tests)
		target_compile_options(${kyty_exception_test} PRIVATE -fexceptions)
	endforeach()
endif()

if(BUILD_TESTING)
	add_test(NAME emulator_user_name_cli
	         COMMAND $<TARGET_FILE:kyty_emulator> --help --user-name 1234567890123456)
	add_test(NAME emulator_user_name_too_long
	         COMMAND $<TARGET_FILE:kyty_emulator> --help --user-name 12345678901234567)
	set_tests_properties(emulator_user_name_too_long PROPERTIES WILL_FAIL TRUE)
	add_test(NAME emulator_present_mode_cli
	         COMMAND $<TARGET_FILE:kyty_emulator> --help --present-mode Immediate)
	add_test(NAME emulator_present_mode_invalid
	         COMMAND $<TARGET_FILE:kyty_emulator> --help --present-mode Invalid)
	set_tests_properties(emulator_present_mode_invalid PROPERTIES WILL_FAIL TRUE)
	add_test(NAME ime_dialog COMMAND $<TARGET_FILE:ime_dialog_tests>)
	add_test(NAME shader_cfg COMMAND $<TARGET_FILE:shader_cfg_tests>)
	add_test(NAME scalar_provenance COMMAND $<TARGET_FILE:scalar_provenance_tests>)
	add_test(NAME image_page_table COMMAND $<TARGET_FILE:image_page_table_tests>)
	add_test(NAME image_cache_gc_policy COMMAND $<TARGET_FILE:image_cache_gc_policy_tests>)
	add_test(NAME ampr_counter_bank COMMAND $<TARGET_FILE:ampr_counter_bank_tests>)
	add_test(NAME draw_prep COMMAND $<TARGET_FILE:draw_prep_tests>)
	add_test(NAME repeat_trace COMMAND $<TARGET_FILE:repeat_trace_tests>)
	add_test(NAME binding_path COMMAND $<TARGET_FILE:binding_path_tests>)
	add_test(NAME image_exact_lookup
	         COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --image-exact-lookup-only)
	add_test(NAME image_pressure_retirement
	         COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --image-pressure-retirement-only)
	add_test(NAME memory_tracker COMMAND $<TARGET_FILE:memory_tracker_tests>)
	add_test(NAME page_manager COMMAND $<TARGET_FILE:page_manager_tests>)
	add_test(NAME bit_array COMMAND $<TARGET_FILE:bit_array_tests>)
	add_test(NAME profiler_counters COMMAND $<TARGET_FILE:profiler_counter_tests>)
	add_test(NAME lod_stats_report COMMAND $<TARGET_FILE:lod_stats_report_tests>)
	add_test(NAME lod_stats_codegen
	         COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --lod-stats-codegen-only)
	add_test(NAME shader_vertex_metadata COMMAND $<TARGET_FILE:shader_vertex_metadata_tests>)
	add_test(NAME resource_materialization COMMAND $<TARGET_FILE:resource_materialization_tests>)
	add_test(NAME resource_tracking COMMAND $<TARGET_FILE:resource_tracking_tests>)
	add_test(NAME event_queue_lifetime COMMAND $<TARGET_FILE:event_queue_lifetime_tests>)
	add_test(NAME kernel_file_system COMMAND $<TARGET_FILE:kernel_file_system_tests>)
	add_test(NAME sync_on_address COMMAND $<TARGET_FILE:sync_on_address_tests>)
	add_test(NAME audio_out2_port COMMAND $<TARGET_FILE:audio_out2_port_tests>)
	add_test(NAME save_data_memory COMMAND $<TARGET_FILE:save_data_memory_tests>)
	add_test(NAME shader_recompiler_compute COMMAND $<TARGET_FILE:shader_recompiler_compute_tests>)
	add_test(NAME shader_recompiler_alignbyte
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --alignbyte-only)
	add_test(NAME graphics_pipeline_rasterization
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --polygon-mode-only)
	add_test(NAME graphics_draw_offsets
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-offset-only)
	add_test(NAME virtual_memory_allocation
		COMMAND $<TARGET_FILE:virtual_memory_allocation_tests>)
	# KYTY_CLAMP_RANGE_MEMO: verify mode and off.
	add_test(NAME virtual_memory_allocation_clamp_memo_verify
		COMMAND $<TARGET_FILE:virtual_memory_allocation_tests>)
	set_tests_properties(virtual_memory_allocation_clamp_memo_verify PROPERTIES
		ENVIRONMENT "KYTY_CLAMP_RANGE_MEMO_VERIFY=exit")
	add_test(NAME virtual_memory_allocation_clamp_memo_off
		COMMAND $<TARGET_FILE:virtual_memory_allocation_tests>)
	set_tests_properties(virtual_memory_allocation_clamp_memo_off PROPERTIES
		ENVIRONMENT "KYTY_CLAMP_RANGE_MEMO=0")
	add_test(NAME guest_red_zone_patcher
		COMMAND $<TARGET_FILE:virtual_memory_allocation_tests> --red-zone-patcher-only)
	add_test(NAME command_scheduler_timeline
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --scheduler-only)
	add_test(NAME stream_buffer_ring
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --stream-buffer-only)
	add_test(NAME gpu_command_lane
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --gpu-command-lane-only)
	add_test(NAME pm4_context_state
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --context-state-only)
	add_test(NAME gpu_tiler
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --gpu-tiler-only)
	add_test(NAME tile_layout
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --tile-layout-only)

	add_test(NAME texture_cache_layered_image
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --layered-image-only)
	add_test(NAME host_image_allocation
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --host-image-allocation-only)
	add_test(NAME texture_cache_image_views
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --image-view-cache-only)
	add_test(NAME texture_cache_storage_sampled
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --storage-sampled-only)
	add_test(NAME texture_cache_depth_readback
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --depth-readback-only)
	add_test(NAME compute_meta_clear_classification
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --compute-meta-clear-only)
	add_test(NAME buffer_cache_dirty_gc
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-cache-gc-only)
	add_test(NAME draw_prep_code_cert
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-code-cert-only)
	add_test(NAME draw_prep_code_cert_bytes
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-code-cert-only)
	set_tests_properties(draw_prep_code_cert_bytes PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP_CODE_DIGEST=0")
	# KYTY_BUFFER_RANGE_MEMO and KYTY_HOT_PAGE_CHECK_LIMIT: default, verify mode, both off.
	add_test(NAME buffer_range_memo
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-range-memo-only)
	add_test(NAME buffer_range_memo_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-range-memo-only)
	set_tests_properties(buffer_range_memo_verify PROPERTIES
		ENVIRONMENT "KYTY_BUFFER_RANGE_MEMO_VERIFY=exit")
	add_test(NAME buffer_range_memo_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-range-memo-only)
	set_tests_properties(buffer_range_memo_off PROPERTIES
		ENVIRONMENT "KYTY_BUFFER_RANGE_MEMO=0;KYTY_HOT_PAGE_CHECK_LIMIT=0")
	# KYTY_BUFFER_DIRTY_QUERY_COMBINED=0: the small-read decision from the two separate queries.
	add_test(NAME buffer_range_memo_separate_queries
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-range-memo-only)
	set_tests_properties(buffer_range_memo_separate_queries PROPERTIES
		ENVIRONMENT "KYTY_BUFFER_RANGE_MEMO=0;KYTY_BUFFER_DIRTY_QUERY_COMBINED=0;KYTY_TRACKER_RELAXED_QUERIES=0")
	# KYTY_TRACKER_RELAXED_QUERIES: verify mode (every lock-free answer checked against the locked
	# queries), and the locked queries with the range memo.
	add_test(NAME buffer_range_memo_relaxed_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-range-memo-only)
	set_tests_properties(buffer_range_memo_relaxed_verify PROPERTIES
		ENVIRONMENT "KYTY_TRACKER_RELAXED_VERIFY=exit")
	add_test(NAME buffer_range_memo_locked_queries
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-range-memo-only)
	set_tests_properties(buffer_range_memo_locked_queries PROPERTIES
		ENVIRONMENT "KYTY_TRACKER_RELAXED_QUERIES=0")
	# KYTY_BDA_SYNC_EPOCH: default, with incremental synchronization (as the product runs), verify
	# mode (it needs the fault epoch of incremental synchronization), off.
	add_test(NAME bda_sync_epoch
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --bda-sync-epoch-only)
	add_test(NAME bda_sync_epoch_incremental
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --bda-sync-epoch-only)
	set_tests_properties(bda_sync_epoch_incremental PROPERTIES
		ENVIRONMENT "KYTY_BDA_INCREMENTAL_SYNC=1")
	add_test(NAME bda_sync_epoch_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --bda-sync-epoch-only)
	set_tests_properties(bda_sync_epoch_verify PROPERTIES
		ENVIRONMENT "KYTY_BDA_INCREMENTAL_SYNC=1;KYTY_BDA_SYNC_EPOCH_VERIFY=exit")
	add_test(NAME bda_sync_epoch_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --bda-sync-epoch-only)
	set_tests_properties(bda_sync_epoch_off PROPERTIES
		ENVIRONMENT "KYTY_BDA_SYNC_EPOCH=0")
	# KYTY_BDA_DIRTY_LOG (with incremental BDA synchronization): verify mode and off.
	add_test(NAME bda_sync_dirty_log_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --bda-sync-epoch-only)
	set_tests_properties(bda_sync_dirty_log_verify PROPERTIES
		ENVIRONMENT "KYTY_BDA_INCREMENTAL_SYNC=1;KYTY_BDA_DIRTY_LOG_VERIFY=exit;KYTY_BDA_HOT_SYNC_VERIFY=exit")
	add_test(NAME bda_sync_dirty_log_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --bda-sync-epoch-only)
	set_tests_properties(bda_sync_dirty_log_off PROPERTIES
		ENVIRONMENT "KYTY_BDA_INCREMENTAL_SYNC=1;KYTY_BDA_DIRTY_LOG=0")
	# KYTY_BINDING_EPOCH_MEMO: default, verify mode, off, and off through KYTY_SYNC_EPOCH=0.
	add_test(NAME binding_epoch_memo
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --binding-epoch-memo-only)
	add_test(NAME binding_epoch_memo_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --binding-epoch-memo-only)
	set_tests_properties(binding_epoch_memo_verify PROPERTIES
		ENVIRONMENT "KYTY_BINDING_EPOCH_MEMO_VERIFY=exit")
	add_test(NAME binding_epoch_memo_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --binding-epoch-memo-only)
	set_tests_properties(binding_epoch_memo_off PROPERTIES
		ENVIRONMENT "KYTY_BINDING_EPOCH_MEMO=0")
	add_test(NAME binding_epoch_memo_no_sync_epoch
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --binding-epoch-memo-only)
	set_tests_properties(binding_epoch_memo_no_sync_epoch PROPERTIES
		ENVIRONMENT "KYTY_SYNC_EPOCH=0")
	add_test(NAME binding_epoch_memo_cross_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --binding-epoch-memo-only)
	set_tests_properties(binding_epoch_memo_cross_off PROPERTIES
		ENVIRONMENT "KYTY_BINDING_MEMO_CROSS_EPOCH=0")
	# KYTY_WRITTEN_SYNC_SKIP: default, verify mode, off.
	add_test(NAME written_sync_skip
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --written-sync-skip-only)
	add_test(NAME written_sync_skip_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --written-sync-skip-only)
	set_tests_properties(written_sync_skip_verify PROPERTIES
		ENVIRONMENT "KYTY_WRITTEN_SYNC_SKIP_VERIFY=exit")
	add_test(NAME written_sync_skip_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --written-sync-skip-only)
	set_tests_properties(written_sync_skip_off PROPERTIES
		ENVIRONMENT "KYTY_WRITTEN_SYNC_SKIP=0")
	# Tracker-gap detectors: the page states they classify host writes by.
	add_test(NAME tracker_gap_detectors
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --tracker-gap-detectors-only)
	add_test(NAME tracker_gap_host_write_tracking_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --tracker-gap-detectors-only)
	set_tests_properties(tracker_gap_host_write_tracking_off PROPERTIES
		ENVIRONMENT "KYTY_HOST_WRITE_TRACKING=0")
	# KYTY_TEXTURE_MEMO_REVALIDATE: default, verify mode, off.
	add_test(NAME texture_memo_revalidate
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --texture-memo-revalidate-only)
	add_test(NAME texture_memo_revalidate_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --texture-memo-revalidate-only)
	set_tests_properties(texture_memo_revalidate_verify PROPERTIES
		ENVIRONMENT "KYTY_TEXTURE_MEMO_REVALIDATE_VERIFY=exit")
	add_test(NAME texture_memo_revalidate_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --texture-memo-revalidate-only)
	set_tests_properties(texture_memo_revalidate_off PROPERTIES
		ENVIRONMENT "KYTY_TEXTURE_MEMO_REVALIDATE=0")
	# KYTY_DRAW_SEQUENCE_FAST: default, off, and the verify mode over the whole compute/renderer
	# suite (every repeated target lookup and stage texture set is checked against the full path).
	# The same run verifies KYTY_RENDER_STATE_FAST (every partial copy and target entry resolved
	# without value-initialisation against the full one).
	add_test(NAME draw_sequence
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-sequence-only)
	add_test(NAME draw_sequence_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-sequence-only)
	set_tests_properties(draw_sequence_off PROPERTIES
		ENVIRONMENT "KYTY_DRAW_SEQUENCE_FAST=0")
	add_test(NAME draw_sequence_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests>)
	set_tests_properties(draw_sequence_verify PROPERTIES
		ENVIRONMENT "KYTY_DRAW_SEQUENCE_VERIFY=exit;KYTY_RENDER_STATE_VERIFY=exit")
	# KYTY_UPLOAD_DMA_HOST_COPY: the DMA worker copies staged guest bytes (default) or the command
	# processor does.
	add_test(NAME upload_dma
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --upload-dma-only)
	add_test(NAME upload_dma_host_copy_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --upload-dma-only)
	set_tests_properties(upload_dma_host_copy_off PROPERTIES
		ENVIRONMENT "KYTY_UPLOAD_DMA_HOST_COPY=0")
	# KYTY_RENDER_STATE_FAST: default and off.
	add_test(NAME render_state
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --render-state-only)
	add_test(NAME render_state_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --render-state-only)
	set_tests_properties(render_state_off PROPERTIES
		ENVIRONMENT "KYTY_RENDER_STATE_FAST=0")
	# Draw-prep end to end: serial vs command-processor draws, in each engine mode.
	add_test(NAME draw_prep_engine
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	add_test(NAME draw_prep_engine_inline
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_inline PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=inline;KYTY_DRAW_PREP_VERIFY=exit")
	add_test(NAME draw_prep_engine_parallel
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_parallel PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=parallel;KYTY_DRAW_PREP_VERIFY=exit")
	add_test(NAME draw_prep_engine_parallel_fence
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_parallel_fence PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=parallel;KYTY_DRAW_PREP_VERIFY=exit;KYTY_DRAW_PREP_REG_INDIRECT_WINDOW=0")
	# Value certificates (byte and digest comparisons), and the log-mode audit on top of them.
	add_test(NAME draw_prep_engine_parallel_value
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_parallel_value PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=parallel;KYTY_DRAW_PREP_VERIFY=exit;KYTY_DRAW_PREP_CERT=value")
	add_test(NAME draw_prep_engine_parallel_value_audit
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_parallel_value_audit PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=parallel;KYTY_DRAW_PREP_VERIFY=exit;KYTY_DRAW_PREP_CERT=value;KYTY_DRAW_PREP_LOG_AUDIT=1")
	# KYTY_BACKING_INPLACE and KYTY_DRAW_PREP_LOCKFREE_HINT: verify modes, and both off.
	add_test(NAME draw_prep_engine_parallel_inplace_verify
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_parallel_inplace_verify PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=parallel;KYTY_DRAW_PREP_VERIFY=exit;KYTY_BACKING_INPLACE_VERIFY=exit;KYTY_DRAW_PREP_LOCKFREE_HINT_VERIFY=exit;KYTY_TRACKER_RELAXED_VERIFY=exit;KYTY_BINDING_EPOCH_MEMO_VERIFY=exit;KYTY_DRAW_SEQUENCE_VERIFY=exit;KYTY_RENDER_STATE_VERIFY=exit")
	add_test(NAME draw_prep_engine_render_state_off
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_render_state_off PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=parallel;KYTY_DRAW_PREP_VERIFY=exit;KYTY_RENDER_STATE_FAST=0")
	add_test(NAME draw_prep_engine_parallel_copied
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-engine-only)
	set_tests_properties(draw_prep_engine_parallel_copied PROPERTIES
		ENVIRONMENT "KYTY_DRAW_PREP=parallel;KYTY_DRAW_PREP_VERIFY=exit;KYTY_BACKING_INPLACE=0;KYTY_DRAW_PREP_LOCKFREE_HINT=0")
	add_test(NAME draw_prep_code_cert_copied
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --draw-prep-code-cert-only)
	set_tests_properties(draw_prep_code_cert_copied PROPERTIES
		ENVIRONMENT "KYTY_BACKING_INPLACE=0")
	add_test(NAME buffer_cache_eager_readback
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --readback-eager-only)

	if(WIN32)
		# These tests still depend on the Windows multisample-depth path.
		add_test(NAME texture_cache_image_overlap
			COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --image-overlap-only)
		add_test(NAME texture_cache_htile_clear
			COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --htile-clear-only)
		add_test(NAME buffer_cache_ranges
			COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-cache-range-only)
	endif()

	# A failed check fails its test from its message too, not only from the exit status (which
	# a harness can lose, or a process exit can overtake when the check runs on another thread).
	# The failure formats of the test programs: "<Suite>: failed: ...", "<Suite>: <case> failed
	# ...", "FAILED: ...", "... test failed: ..." and "SaveDataMemoryTests:<line>: ...".
	# failure_message_fails_test checks the expression itself: a program that prints a failure
	# and exits with status 0 must fail (inverted by WILL_FAIL).
	add_test(NAME failure_message_fails_test
		COMMAND ${CMAKE_COMMAND} -E echo "SelfTests: failed: a check failed, the process exited 0")
	set_tests_properties(failure_message_fails_test PROPERTIES WILL_FAIL TRUE)
	get_property(kyty_test_names DIRECTORY PROPERTY TESTS)
	set_tests_properties(${kyty_test_names} PROPERTIES FAIL_REGULAR_EXPRESSION
		"Tests: failed: ;Tests: [^\n]* failed at ;Tests: [^\n]* failed: ;FAILED: ;test failed: ;SaveDataMemoryTests:[0-9]+: ")

	add_custom_target(kyty_tests DEPENDS
		ime_dialog_tests
		shader_cfg_tests
		scalar_provenance_tests
		image_page_table_tests
		memory_tracker_tests
		page_manager_tests
		bit_array_tests
		lod_stats_report_tests
		shader_vertex_metadata_tests
		resource_materialization_tests
		resource_tracking_tests
		binding_path_tests
		event_queue_lifetime_tests
		kernel_file_system_tests
		sync_on_address_tests
		shader_recompiler_compute_tests
		virtual_memory_allocation_tests
		audio_out2_port_tests
	)
endif()

# Test Darwin signal-context restoration for SSE4a instructions.
if(APPLE AND (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR
		(NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")))
	add_executable(macos_sse4a_tests EXCLUDE_FROM_ALL
		"${KYTY_TESTS_DIR}/macosSse4aTests.cpp"
		"${KYTY_SOURCE_DIR}/loader/x64InstructionEmulator.cpp"
	)
	target_link_libraries(macos_sse4a_tests common fmt::fmt Zydis::Zydis)
	target_include_directories(macos_sse4a_tests PRIVATE ${inc_headers})
	if(BUILD_TESTING)
		add_test(NAME macos_sse4a COMMAND $<TARGET_FILE:macos_sse4a_tests>)
		set_tests_properties(macos_sse4a PROPERTIES TIMEOUT 15)
		add_dependencies(kyty_tests macos_sse4a_tests)
	endif()
endif()

add_executable(kyty_emulator "${KYTY_SOURCE_DIR}/main.cpp" ${kyty_emulator_src})
configure_macos_guest_address_space(kyty_emulator)

option(KYTY_EMULATOR_IPO "Enable release interprocedural optimization for the emulator target" OFF)
if(KYTY_EMULATOR_IPO)
	set_property(TARGET kyty_emulator PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

target_link_libraries(kyty_emulator ${kyty_emulator_link_libraries})
if (WIN32)
	target_link_libraries(kyty_emulator opengl32)
	target_link_libraries(kyty_emulator iphlpapi)
	target_link_libraries(kyty_emulator setupapi)
	target_link_libraries(kyty_emulator ws2_32)
endif()
if (KYTY_CLANG_CL)
	target_link_libraries(kyty_emulator onecore)
endif()
if (CLANG AND NOT KYTY_CLANG_CL)
	target_link_libraries(kyty_emulator pthread)
endif()
# dlopen/dlsym/dladdr for RenderDoc.
target_link_libraries(kyty_emulator ${CMAKE_DL_LIBS})
target_include_directories(kyty_emulator PRIVATE ${inc_headers})

clang_tidy_check(kyty_emulator "" "${check_headers}" "${inc_headers}")

include_what_you_use(kyty_emulator "${inc_headers}")

set(KYTY_EMULATOR_MAP_NAME "kyty_emulator_${KYTY_COMPILER_ID}_${KYTY_LINKER_ID}.map")

set(KYTY_EMULATOR_MAP_LINK_PATH "${CMAKE_CURRENT_BINARY_DIR}/${KYTY_EMULATOR_MAP_NAME}")
set(KYTY_EMULATOR_PDB_LINK_PATH "${CMAKE_CURRENT_BINARY_DIR}/kyty_emulator.pdb")

if(KYTY_CLANG_CL)
	target_link_options(kyty_emulator PRIVATE
		"/DYNAMICBASE:NO"
		"/DEBUG:FULL"
		"/PDB:${KYTY_EMULATOR_PDB_LINK_PATH}"
		"/lldmap:${KYTY_EMULATOR_MAP_LINK_PATH}"
	)
	add_custom_command(TARGET kyty_emulator POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" $<TARGET_FILE_DIR:kyty_emulator>/libwinpthread-1.dll)
elseif(WIN32 OR LINUX)
	set_target_properties(kyty_emulator PROPERTIES LINK_FLAGS "${KYTY_LD_OPTIONS}")
	# LINK_FLAGS is a space-delimited string: a -Map path containing spaces would
	# be split into bogus arguments, so pass it as a discrete link option instead.
	target_link_options(kyty_emulator PRIVATE "-Wl,-Map=${KYTY_EMULATOR_MAP_LINK_PATH}")
endif()

if(LINUX)
    target_link_options(kyty_emulator PRIVATE
        $<$<CONFIG:Release>:-Wl,--strip-all>
    )
endif()

if(APPLE)
	# The emulator writes x86-64 trampolines/PLT stubs into memory and executes them, and
	# runs guest code under Rosetta. That requires the JIT / unsigned-executable-memory
	# entitlements, so re-sign the binary after every link (an unsigned/relinked binary
	# reverts to the hardened defaults and aborts when it first executes written code).
	add_custom_command(TARGET kyty_emulator POST_BUILD
		COMMAND codesign -s - --force --options runtime
			--entitlements "${KYTY_SOURCE_DIR}/macos_jit.entitlements"
			$<TARGET_FILE:kyty_emulator>
		COMMENT "Codesign kyty_emulator with JIT entitlements (macOS)")
endif()

install(TARGETS kyty_emulator DESTINATION .)
if(KYTY_BUILD_LAUNCHER)
	add_subdirectory("${KYTY_SOURCE_DIR}/launcher" "${CMAKE_CURRENT_BINARY_DIR}/launcher")
endif()
if(KYTY_CLANG_CL)
	install(FILES "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" DESTINATION .)
endif()
