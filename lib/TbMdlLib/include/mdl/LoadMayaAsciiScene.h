/*
 Copyright (C) 2026 Nachtmaschine contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Result.h"

#include "vm/vec.h"

#include <filesystem>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace tb::mdl
{
class EntityNode;
class Map;

/**
 * One map entity to spawn from a Maya ASCII (.ma) transform / locator.
 * Positions and rotations are in TrenchBroom world space (Z-up).
 */
struct MayaAsciiEntitySpawn
{
  std::string classname;
  vm::vec3d origin{0, 0, 0};
  std::string angles; // "pitch yaw roll" in degrees, idTech style
  std::map<std::string, std::string> extraProperties;
};

bool canLoadMayaAsciiScene(const std::filesystem::path& path);

Result<std::vector<MayaAsciiEntitySpawn>> loadMayaAsciiScene(std::istream& stream);

/**
 * Adds entity nodes to the map under the current layer / group parent.
 * Caller should wrap in a Transaction.
 *
 * @return the created entity nodes (empty if nothing to import).
 */
std::vector<EntityNode*> importMayaAsciiSceneIntoMap(
  Map& map, const std::vector<MayaAsciiEntitySpawn>& spawns);

} // namespace tb::mdl
