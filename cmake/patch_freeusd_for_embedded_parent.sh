#!/usr/bin/env sh
# When FreeUSD is added via FetchContent, CMAKE_SOURCE_DIR in its CMakeLists.txt
# resolves to the parent project (the editor). Replace with PROJECT_SOURCE_DIR so
# package templates, LICENSE, and README resolve inside the FreeUSD tree.
set -e
if [ ! -f CMakeLists.txt ]; then
  echo "patch_freeusd_for_embedded_parent: expected CMakeLists.txt in $(pwd)" >&2
  exit 1
fi
perl -pi -e 's/\$\{CMAKE_SOURCE_DIR\}\/cmake/\$\{PROJECT_SOURCE_DIR\}\/cmake/g' CMakeLists.txt
perl -pi -e 's/\$\{CMAKE_SOURCE_DIR\}\/LICENSE/\$\{PROJECT_SOURCE_DIR\}\/LICENSE/g' CMakeLists.txt
perl -pi -e 's/\$\{CMAKE_SOURCE_DIR\}\/README\.md/\$\{PROJECT_SOURCE_DIR\}\/README.md/g' CMakeLists.txt
