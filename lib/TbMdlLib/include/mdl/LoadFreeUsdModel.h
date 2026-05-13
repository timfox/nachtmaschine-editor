/*
 Nachtmaschine — optional FreeUSD-backed previews for Universal Scene Description files.
*/

#pragma once

#include "Result.h"
#include "mdl/EntityModel.h"

#include <filesystem>

namespace tb
{
class Logger;

namespace fs
{
class FileSystem;
}

namespace mdl
{

bool canLoadFreeUsdModel(const std::filesystem::path& path);

Result<EntityModelData> loadFreeUsdModel(
  const std::filesystem::path& path, const fs::FileSystem& fs, Logger& logger);

} // namespace mdl
} // namespace tb
