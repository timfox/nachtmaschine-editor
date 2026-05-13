# Optional FreeUSD (GPL-3.0-or-later) — https://github.com/gopexllc/freeusd
# Used for Universal Scene Description previews in the entity model panel.
option(NACHT_EDITOR_WITH_FREEUSD "Fetch and link FreeUSD for USD (.usd/.usda/.usdc) entity previews" ON)

if(NACHT_EDITOR_WITH_FREEUSD)
  include(FetchContent)
  set(FREEUSD_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
  set(FREEUSD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(FREEUSD_BUILD_C_ABI OFF CACHE BOOL "" FORCE)
  set(FREEUSD_TEST_INSTALL_INTEGRATION OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    nacht_freeusd
    GIT_REPOSITORY https://github.com/gopexllc/freeusd.git
    GIT_TAG 4b7d10fb9709031b32469a4180b6da8b11968b0e
    PATCH_COMMAND "${CMAKE_CURRENT_LIST_DIR}/patch_freeusd_for_embedded_parent.sh"
  )
  FetchContent_MakeAvailable(nacht_freeusd)
endif()
