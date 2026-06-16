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

#include "vm/bbox.h"
#include "vm/vec.h"

#include <filesystem>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace tb::mdl
{
class Map;
class Node;

/**
 * How a tagged Maya transform / locator should appear in the map.
 *
 * Naming (on transform or locator):
 *   tb_entity_<classname> / entity_<classname> / tb_spawn_<classname> — point entity
 *   tb_model_<path> or tb_model attribute — misc_model (not inferred from untagged mesh)
 *   tb_trigger_<classname> — brush entity (default classname trigger_once)
 *   tb_brush_<classname> — brush entity (mesh → convex hull; fallback axis-aligned box)
 *   tb_brush_box_<classname> — brush entity forced to mesh axis-aligned box
 *   tb_brush_hull_<classname> — brush entity forced to convex hull from mesh vertices
 *   tb_locator_<classname> — point entity from a locator
 *
 * Attributes: tb_class, tb_model, tb_material, tb_kind (entity|model|brush|trigger),
 *   tb_brush_shape (box|hull|aabb — optional brush volume mode)
 */
enum class MayaAsciiImportKind
{
  Point,
  Brush,
};

enum class MayaBrushShapeMode
{
  Auto,
  AxisAlignedBox,
  ConvexHull,
};

struct MayaAsciiEntitySpawn
{
  MayaAsciiImportKind kind = MayaAsciiImportKind::Point;
  std::string classname;
  vm::vec3d origin{0, 0, 0};
  std::string angles; // "pitch yaw roll" in degrees, engine style
  std::map<std::string, std::string> extraProperties;
  MayaBrushShapeMode brushShapeMode = MayaBrushShapeMode::Auto;
  vm::bbox3d brushBounds{};
  bool hasBrushBounds = false;
  std::vector<vm::vec3d> brushHullVertices;
  std::string brushMaterial = "common/caulk";
};

bool canLoadMayaAsciiScene(const std::filesystem::path& path);

Result<std::vector<MayaAsciiEntitySpawn>> loadMayaAsciiScene(std::istream& stream);

/**
 * Adds imported nodes to the map under the current layer / group parent.
 * Caller should wrap in a Transaction.
 *
 * @return created entity and brush nodes (empty if nothing to import).
 */
std::vector<Node*> importMayaAsciiSceneIntoMap(
  Map& map, const std::vector<MayaAsciiEntitySpawn>& spawns);

} // namespace tb::mdl
