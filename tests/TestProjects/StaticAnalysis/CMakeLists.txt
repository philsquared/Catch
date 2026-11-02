cmake_minimum_required(VERSION 3.16)
project(static-analysis-tests LANGUAGES CXX)

# std::optional, which is what bugprone-unchecked-optional-access reasons about
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(${CATCH2_PATH} catch2-build)

set(SA_TEST_SOURCES
  RequireTerminates.cpp
  CheckedIf.cpp
  NoThrowTerminates.cpp
  ThrowsTerminates.cpp
  ThrowsAsTerminates.cpp
)

# These TUs compile but never link. 
# Per-target clang-tidy keeps Catch2 itself out of the analysis.
foreach(sourceFile ${SA_TEST_SOURCES})
  get_filename_component(targetName "${sourceFile}" NAME_WE)
  add_library(${targetName} OBJECT "${sourceFile}")
  target_link_libraries(${targetName} PRIVATE Catch2::Catch2)
  set_target_properties(${targetName}
    PROPERTIES
      CXX_CLANG_TIDY "${STATIC_ANALYSIS_CLANG_TIDY}"
  )
endforeach()
