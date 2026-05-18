# TrenchBroom Coding Agent Guidance

## Project structure
- The project contains several applications under /app. The main application target is TrenchBroom.
- Most code lives in libraries under /lib.
- **Nachtmaschine engine material extensions** (e.g. SPOM in `.mtr` files) are documented for authors in **`docs/Materials-SPOM.md`**; the runtime implementation lives in the parent **Nachtmaschine** game repository, not in this editor’s C++ tree.
- Each application target and library target has its own CMakeLists.txt file.
- Shared CMake utilities are in /cmake.
- Each library usually has a <Name>LibTest target for its tests, for example TbMdlLibTest.
- **Maya ASCII level import:** **File → Import Maya Scene…** reads `.ma` transform/locator nodes into map entities (`mdl/LoadMayaAsciiScene.*`). Author with `tb_entity_<classname>` or `entity_<classname>` names, optional `tb_class` / `tb_model` string attrs, or mesh children → `misc_model`. Maya Y-up positions are converted to TrenchBroom Z-up.
- **Default input (Maya-oriented fork):** 3D camera defaults to **Alt+LMB / Alt+MMB / Alt+RMB** (`Preferences::CameraNavigationScheme`, **Preferences → Mouse → Mouse scheme**). 2D views use the same scheme where **Alt+RMB** is zoom-drag and **Alt+MMB** pans when “Alt + middle mouse drag to move camera” is on; **RMB** still pans without Alt. Tool keys default to **Q** (deactivate), **E/R** (rotate/scale), **F** (focus selection), **Shift+E/F/G** for edge/face/shear; fly **up** defaults to **Z** so **Q** is free.
- Some libraries also have a <Name>TestUtilsLib target for shared test helpers, and some of those have a matching <Name>TestUtilsLibTest target.

## Build and test
- **CI (parent repo):** GitHub Actions job **`Linux editor (TbMdlLib + TbUiLib)`** configures **`editor/`** with **`NACHT_EDITOR_WITH_FREEUSD=OFF`** (no FreeUSD fetch) and builds **`TbMdlLib`** + **`TbUiLib`** to guard Qt6 / WebSockets / model-loader changes. That job caches **`editor/vcpkg/downloads`** and **`editor-build-ci/vcpkg_installed`** (keyed on **`editor/vcpkg.json`**) to speed repeat CI runs.
- **Nacht fork extras:** CMake requires **Qt6 Svg** and **Qt6 WebSockets**. On Debian/Ubuntu install **`libqt6svg6-dev`** and **`qt6-websockets-dev`** (names may vary). If CMake still cannot find a module, pass **`Qt6Svg_DIR`** / **`Qt6WebSockets_DIR`** (see **`editor/scripts/configure-linux-nacht.sh`** env **`QT6SVG_DIR`** / **`QT6WEBSOCKETS_DIR`**). Optional **FreeUSD** for USD (`.usd` / `.usda` / `.usdc`) entity previews is controlled by **`NACHT_EDITOR_WITH_FREEUSD`** (default ON; fetches [freeusd](https://github.com/gopexllc/freeusd) at configure time — needs network once). **`bash editor/scripts/configure-linux-nacht.sh`** passes **`NACHT_EDITOR_WITH_FREEUSD=ON`** by default; set **`NACHT_EDITOR_WITH_FREEUSD=0`** to disable. After configure, the editor listens for automation WebSockets only when started with **`--editor-ws-port <port>`** or **`NACHT_TB_WEBSOCKET_PORT`** (see `app/TrenchBroom/src/Main.cpp`).
- TrenchBroom uses CMake as its build system.
- In Visual Studio Code, prefer CMake Tools for builds.
- Build the narrowest relevant target instead of building the whole workspace when possible.
- For library changes, prefer the corresponding <Name>LibTest target to validate the change.
- Always build the relevant test target before running tests.
- Tests use Catch2.
- If VS Code test discovery is unavailable, run the built test executable directly from the build tree, for example build/lib/TbMdlLib/test/TbMdlLibTest.
- Use --list-tests to discover available tests and Catch2 filters to run a focused subset.
- Use Build.md for platform-specific setup and dependency details.

### Code coverage
- **Enable coverage instrumentation**: Pass `-DTB_ENABLE_GCOV=1` for gcov-compatible coverage (works with GCC or Clang) or `-DTB_ENABLE_LCOV=1` for LLVM source-based coverage (Clang only).
- **Generate coverage data**: 
  - For gcov: Build and run tests normally. `.gcno` and `.gcda` files are automatically generated in the build tree.
  - For LLVM/lcov: Run tests with `LLVM_PROFILE_FILE=default.profraw <test-executable>` to generate `.profraw` profile data.
- **Analyze coverage**: Use coverage tools to identify uncovered code paths, untested branches, and low-coverage functions.
- **Guide test improvements**: When reviewing or creating tests, examine coverage reports to identify and address gaps:
  - Suggest new tests for uncovered code paths or error conditions.
  - Improve existing tests to cover branch conditions not yet exercised.
  - Identify edge cases or exception handling that lack test coverage.
- **Reference coverage in commit messages**: When submitting test improvements motivated by coverage analysis, mention that coverage-guided testing was used to identify gaps.

## Test structure
- For each compilation unit, tests are usually in one file named tst_<CompilationUnit>.cpp.
- Prefer one test case per class.
- Prefer one section per member function.
- For free functions, prefer one test case per file and one section per function.

## Code style
- Format changes with clang-format. The repository style is defined in /.clang-format.
- Respect the existing include ordering rules from /.clang-format. In particular, Qt headers must come first.
- Follow the surrounding file's style and patterns unless there is a clear reason not to.

## Git History
- Keep the git history as clean as possible.
- Avoid unnecessary churn, including changing the same code multiple times in a branch when a cleaner edit is possible.
- Prefer changes that read like a clean transformation from the original state to the desired result.
- When creating a series of commits, keep each commit coherent, buildable, and with the relevant tests passing when practical.
- When asked to write commit messages, explain why the change was made in the context of a feature or bug fix, not just what changed.