o3de_pal_dir(pal_dir ${CMAKE_CURRENT_LIST_DIR}/Platform/${PAL_PLATFORM_NAME} "${gem_restricted_path}" "${gem_path}" "${gem_parent_relative_path}")
include(${pal_dir}/PAL_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)

if(NOT PAL_TRAIT_STWGAMEPLAY_SUPPORTED)
    return()
endif()

ly_add_target(
    NAME STWGameplay.API INTERFACE
    NAMESPACE Gem
    FILES_CMAKE stwgameplay_api_files.cmake
    INCLUDE_DIRECTORIES INTERFACE Include
    BUILD_DEPENDENCIES INTERFACE AZ::AzCore)

ly_add_target(
    NAME STWGameplay.Private.Object STATIC
    NAMESPACE Gem
    FILES_CMAKE stwgameplay_private_files.cmake
    TARGET_PROPERTIES O3DE_PRIVATE_TARGET TRUE
    INCLUDE_DIRECTORIES PRIVATE Include Source
    BUILD_DEPENDENCIES PUBLIC AZ::AzCore AZ::AzFramework Gem::Atom_Feature_Common.Public)

ly_add_target(
    NAME STWGameplay ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    FILES_CMAKE stwgameplay_shared_files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include PRIVATE Source
    BUILD_DEPENDENCIES PUBLIC Gem::STWGameplay.API PRIVATE Gem::STWGameplay.Private.Object)

ly_add_source_properties(
    SOURCES Source/Clients/STWGameplayModule.cpp
    PROPERTY COMPILE_DEFINITIONS
    VALUES O3DE_GEM_NAME=STWGameplay O3DE_GEM_VERSION=1.0.0)

ly_create_alias(NAME STWGameplay.Clients NAMESPACE Gem TARGETS Gem::STWGameplay)
ly_create_alias(NAME STWGameplay.Unified NAMESPACE Gem TARGETS Gem::STWGameplay)
ly_create_alias(NAME STWGameplay.Clients.API NAMESPACE Gem TARGETS Gem::STWGameplay.API)
ly_create_alias(NAME STWGameplay.Unified.API NAMESPACE Gem TARGETS Gem::STWGameplay.API)

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND PAL_TRAIT_STWGAMEPLAY_TEST_SUPPORTED)
    ly_add_target(
        NAME STWGameplay.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        FILES_CMAKE stwgameplay_tests_files.cmake
        INCLUDE_DIRECTORIES PRIVATE Tests Source Include
        BUILD_DEPENDENCIES PRIVATE AZ::AzTest AZ::AzFramework Gem::STWGameplay.Private.Object)
    ly_add_googletest(NAME Gem::STWGameplay.Tests)
endif()
