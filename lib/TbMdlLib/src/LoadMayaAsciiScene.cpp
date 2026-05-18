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

#include "mdl/LoadMayaAsciiScene.h"

#include "mdl/Brush.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushNode.h"
#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/EntityProperties.h"
#include "mdl/EntityRotation.h"
#include "mdl/Map.h"
#include "mdl/Map_Nodes.h"
#include "mdl/WorldNode.h"
#include "kd/k.h"
#include "kd/path_utils.h"

#include "vm/bbox.h"
#include "vm/mat_ext.h"
#include "vm/vec.h"

#include <cctype>
#include <cstring>
#include <istream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace tb::mdl
{
namespace
{

constexpr std::string_view MayaAsciiMagic = "//Maya ASCII";
constexpr double DefaultBrushHalfExtent = 16.0;

enum class MayaObjectRole
{
  None,
  PointEntity,
  ModelEntity,
  BrushEntity,
};

struct MayaNode
{
  std::string name;
  std::optional<std::string> parent;
  vm::vec3d translation{0, 0, 0};
  vm::vec3d rotationDeg{0, 0, 0};
  vm::vec3d scale{1, 1, 1};
  bool hasMeshChild = false;
  bool isLocator = false;
  std::map<std::string, std::string> stringAttrs;
};

struct MayaMeshShape
{
  std::string name;
  std::optional<std::string> parentTransform;
  std::vector<vm::vec3d> localVertices;
};

bool isIgnoredMayaNodeName(const std::string& name)
{
  static const auto ignored = std::unordered_set<std::string>{
    "persp",
    "top",
    "front",
    "side",
    "bottom",
    "back",
    "left",
    "defaultRenderLayer",
    "defaultLightSet",
    "defaultObjectSet",
    "shared",
    "renderPartition",
    "layerManager",
    "lightLinker1",
    "shapeEditorManager",
    "pose1",
    "uiConfigurationScriptNode",
    "hardwareRenderingGlobals",
    "defaultTextureList1",
  };
  return ignored.contains(name);
}

std::string trim(std::string_view str)
{
  auto begin = str.begin();
  while (begin != str.end() && std::isspace(static_cast<unsigned char>(*begin)))
  {
    ++begin;
  }
  auto end = str.end();
  while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))))
  {
    --end;
  }
  return std::string{begin, end};
}

bool looksLikeNumericContinuation(std::string_view line)
{
  if (line.empty())
  {
    return false;
  }
  const auto c = line.front();
  return c == '-' || c == '.' || std::isdigit(static_cast<unsigned char>(c));
}

std::optional<std::string> parseQuotedName(std::string_view line, const std::string_view token)
{
  const auto pos = line.find(token);
  if (pos == std::string_view::npos)
  {
    return std::nullopt;
  }
  const auto quote = line.find('"', pos + token.size());
  if (quote == std::string_view::npos)
  {
    return std::nullopt;
  }
  const auto endQuote = line.find('"', quote + 1);
  if (endQuote == std::string_view::npos)
  {
    return std::nullopt;
  }
  return std::string{line.substr(quote + 1, endQuote - quote - 1)};
}

std::vector<std::string> parseQuotedStrings(std::string_view line)
{
  std::vector<std::string> result;
  auto pos = line.find('"');
  while (pos != std::string_view::npos)
  {
    const auto endQuote = line.find('"', pos + 1);
    if (endQuote == std::string_view::npos)
    {
      break;
    }
    result.emplace_back(line.substr(pos + 1, endQuote - pos - 1));
    pos = line.find('"', endQuote + 1);
  }
  return result;
}

std::optional<vm::vec3d> parseDouble3(std::string_view line)
{
  const auto typeToken = "-type \"double3\"";
  const auto typePos = line.find(typeToken);
  if (typePos == std::string_view::npos)
  {
    return std::nullopt;
  }
  const auto valuesPos = line.find_first_of("0123456789.-", typePos + std::strlen(typeToken));
  if (valuesPos == std::string_view::npos)
  {
    return std::nullopt;
  }
  std::istringstream stream{std::string{line.substr(valuesPos)}};
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  stream >> x >> y >> z;
  if (!stream)
  {
    return std::nullopt;
  }
  return vm::vec3d{x, y, z};
}

void appendFloat3Vertices(std::string_view line, std::vector<vm::vec3d>& vertices)
{
  auto typePos = line.find("-type \"float3\"");
  if (typePos == std::string_view::npos)
  {
    typePos = line.find("-type \"double3\"");
  }

  size_t valuesPos = 0;
  if (typePos != std::string_view::npos)
  {
    valuesPos = line.find_first_of("0123456789.-", typePos + 14);
  }
  else
  {
    valuesPos = line.find_first_of("0123456789.-");
  }

  if (valuesPos == std::string_view::npos)
  {
    return;
  }

  std::istringstream stream{std::string{line.substr(valuesPos)}};
  while (stream)
  {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    stream >> x >> y >> z;
    if (!stream)
    {
      break;
    }
    vertices.emplace_back(x, y, z);
  }
}

std::optional<std::string> parseStringAttr(std::string_view line)
{
  const auto typePos = line.find("-type \"string\"");
  if (typePos == std::string_view::npos)
  {
    return std::nullopt;
  }
  const auto firstQuote = line.find('"', typePos + 14);
  if (firstQuote == std::string_view::npos)
  {
    return std::nullopt;
  }
  const auto secondQuote = line.find('"', firstQuote + 1);
  if (secondQuote == std::string_view::npos)
  {
    return std::nullopt;
  }
  return std::string{line.substr(firstQuote + 1, secondQuote - firstQuote - 1)};
}

vm::vec3d mayaTranslationToWorld(const vm::vec3d& t)
{
  return vm::vec3d{t.x(), -t.z(), t.y()};
}

vm::mat4x4d mayaLocalTransform(
  const vm::vec3d& translation, const vm::vec3d& rotationDeg, const vm::vec3d& scale)
{
  const auto t = mayaTranslationToWorld(translation);
  auto scaleM = vm::mat4x4d::identity();
  scaleM[0][0] = scale.x();
  scaleM[1][1] = scale.y();
  scaleM[2][2] = scale.z();
  const auto rx = vm::to_radians(rotationDeg.x());
  const auto ry = vm::to_radians(rotationDeg.y());
  const auto rz = vm::to_radians(rotationDeg.z());
  const auto rMaya =
    vm::rotation_matrix(vm::vec3d{1, 0, 0}, rx) * vm::rotation_matrix(vm::vec3d{0, 1, 0}, ry)
    * vm::rotation_matrix(vm::vec3d{0, 0, 1}, rz);
  auto B = vm::mat4x4d::identity();
  B[0] = vm::vec4d{1, 0, 0, 0};
  B[1] = vm::vec4d{0, 0, -1, 0};
  B[2] = vm::vec4d{0, 1, 0, 0};
  const auto rWorld = B * rMaya * vm::transpose(B);
  return vm::translation_matrix(t) * rWorld * scaleM;
}

vm::mat4x4d worldTransform(
  const std::string& name,
  const std::unordered_map<std::string, MayaNode>& nodes,
  std::unordered_map<std::string, vm::mat4x4d>& cache)
{
  const auto cacheIt = cache.find(name);
  if (cacheIt != cache.end())
  {
    return cacheIt->second;
  }

  const auto nodeIt = nodes.find(name);
  if (nodeIt == nodes.end())
  {
    return vm::mat4x4d::identity();
  }

  const auto& node = nodeIt->second;
  const auto local = mayaLocalTransform(node.translation, node.rotationDeg, node.scale);
  const auto world = node.parent
                       ? worldTransform(*node.parent, nodes, cache) * local
                       : local;
  cache.emplace(name, world);
  return world;
}

bool startsWith(std::string_view str, std::string_view prefix)
{
  return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

bool hasPrefix(const std::string& str, const std::string& prefix)
{
  return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

std::string stripPrefix(const std::string& str, const std::string& prefix)
{
  if (hasPrefix(str, prefix))
  {
    return str.substr(prefix.size());
  }
  return str;
}

std::optional<std::string> stringAttr(const MayaNode& node, const std::string& key)
{
  const auto it = node.stringAttrs.find(key);
  if (it != node.stringAttrs.end() && !it->second.empty())
  {
    return it->second;
  }
  return std::nullopt;
}

MayaObjectRole resolveObjectRole(const MayaNode& node)
{
  if (const auto kind = stringAttr(node, "tb_kind"))
  {
    if (*kind == "brush" || *kind == "trigger")
    {
      return MayaObjectRole::BrushEntity;
    }
    if (*kind == "model")
    {
      return MayaObjectRole::ModelEntity;
    }
    if (*kind == "entity")
    {
      return MayaObjectRole::PointEntity;
    }
  }

  const auto& name = node.name;
  if (
    hasPrefix(name, "tb_trigger_") || hasPrefix(name, "tb_brush_")
    || hasPrefix(name, "tb_brush_hull_") || hasPrefix(name, "tb_brush_box_"))
  {
    return MayaObjectRole::BrushEntity;
  }
  if (hasPrefix(name, "tb_model_"))
  {
    return MayaObjectRole::ModelEntity;
  }
  if (
    hasPrefix(name, "tb_entity_") || hasPrefix(name, "entity_") || hasPrefix(name, "tb_spawn_")
    || hasPrefix(name, "tb_locator_"))
  {
    return MayaObjectRole::PointEntity;
  }
  if (stringAttr(node, "tb_class"))
  {
    return MayaObjectRole::PointEntity;
  }
  if (stringAttr(node, "tb_model"))
  {
    return MayaObjectRole::ModelEntity;
  }
  if (node.isLocator && hasPrefix(name, "tb_"))
  {
    return MayaObjectRole::PointEntity;
  }

  return MayaObjectRole::None;
}

std::string resolveClassname(const MayaNode& node, MayaObjectRole role)
{
  if (const auto cls = stringAttr(node, "tb_class"))
  {
    return *cls;
  }

  const auto& name = node.name;
  if (hasPrefix(name, "tb_entity_"))
  {
    return stripPrefix(name, "tb_entity_");
  }
  if (hasPrefix(name, "entity_"))
  {
    return stripPrefix(name, "entity_");
  }
  if (hasPrefix(name, "tb_spawn_"))
  {
    return stripPrefix(name, "tb_spawn_");
  }
  if (hasPrefix(name, "tb_locator_"))
  {
    return stripPrefix(name, "tb_locator_");
  }
  if (hasPrefix(name, "tb_trigger_"))
  {
    const auto suffix = stripPrefix(name, "tb_trigger_");
    if (suffix.empty())
    {
      return "trigger_once";
    }
    if (hasPrefix(suffix, "trigger_"))
    {
      return suffix;
    }
    return "trigger_" + suffix;
  }
  if (hasPrefix(name, "tb_brush_hull_"))
  {
    const auto suffix = stripPrefix(name, "tb_brush_hull_");
    return suffix.empty() ? "func_static" : suffix;
  }
  if (hasPrefix(name, "tb_brush_box_"))
  {
    const auto suffix = stripPrefix(name, "tb_brush_box_");
    return suffix.empty() ? "func_static" : suffix;
  }
  if (hasPrefix(name, "tb_brush_"))
  {
    const auto suffix = stripPrefix(name, "tb_brush_");
    return suffix.empty() ? "func_static" : suffix;
  }
  if (hasPrefix(name, "tb_model_"))
  {
    return "misc_model";
  }

  if (role == MayaObjectRole::ModelEntity)
  {
    return "misc_model";
  }

  return {};
}

bool shouldImportNode(const MayaNode& node, MayaObjectRole role, const std::string& classname)
{
  if (role == MayaObjectRole::None || classname.empty() || isIgnoredMayaNodeName(node.name))
  {
    return false;
  }

  if (role == MayaObjectRole::BrushEntity)
  {
    return true;
  }

  if (role == MayaObjectRole::ModelEntity)
  {
    return node.hasMeshChild || stringAttr(node, "tb_model").has_value()
           || hasPrefix(node.name, "tb_model_");
  }

  return true;
}

std::string formatAngles(const vm::mat4x4d& worldMatrix)
{
  const auto rot = vm::strip_translation(worldMatrix);
  const auto pyr = entityYawPitchRoll(vm::mat4x4d::identity(), rot);
  return kdl::str_to_string(pyr.x()) + " " + kdl::str_to_string(pyr.y()) + " "
         + kdl::str_to_string(pyr.z());
}

std::optional<std::string> modelPathForNode(const MayaNode& node, MayaObjectRole role)
{
  if (role != MayaObjectRole::ModelEntity)
  {
    return std::nullopt;
  }

  if (const auto modelIt = stringAttr(node, "tb_model"))
  {
    return modelIt;
  }
  if (const auto modelIt2 = stringAttr(node, "model"))
  {
    return modelIt2;
  }
  if (hasPrefix(node.name, "tb_model_"))
  {
    const auto path = stripPrefix(node.name, "tb_model_");
    if (!path.empty())
    {
      return path;
    }
  }
  return std::nullopt;
}

std::string brushMaterialForNode(const MayaNode& node)
{
  if (const auto mat = stringAttr(node, "tb_material"))
  {
    return *mat;
  }
  if (hasPrefix(node.name, "tb_trigger_"))
  {
    return "common/caulk";
  }
  return "common/caulk";
}

std::optional<vm::bbox3d> meshWorldBounds(
  const std::string& transformName,
  const vm::mat4x4d& world,
  const std::unordered_map<std::string, MayaMeshShape>& meshes)
{
  auto bounds = vm::bbox3d{};
  auto any = false;

  for (const auto& [meshName, mesh] : meshes)
  {
    (void)meshName;
    if (mesh.parentTransform != transformName || mesh.localVertices.empty())
    {
      continue;
    }

    for (const auto& local : mesh.localVertices)
    {
      const auto worldPt = world * vm::vec4d{local.x(), local.y(), local.z(), 1.0};
      const auto pt = vm::vec3d{worldPt.x(), worldPt.y(), worldPt.z()};
      if (!any)
      {
        bounds = vm::bbox3d{pt, pt};
        any = true;
      }
      else
      {
        bounds = merge(bounds, vm::bbox3d{pt, pt});
      }
    }
  }

  if (!any)
  {
    return std::nullopt;
  }
  return bounds;
}

vm::bbox3d defaultBrushBounds(const vm::vec3d& origin)
{
  const auto half = vm::vec3d{
    DefaultBrushHalfExtent, DefaultBrushHalfExtent, DefaultBrushHalfExtent};
  return vm::bbox3d{origin - half, origin + half};
}

constexpr double HullVertexMergeEpsilon = 1e-3;

bool pointsNear(const vm::vec3d& a, const vm::vec3d& b)
{
  return vm::squared_length(a - b) < HullVertexMergeEpsilon * HullVertexMergeEpsilon;
}

std::vector<vm::vec3d> meshWorldVertices(
  const std::string& transformName,
  const vm::mat4x4d& world,
  const std::unordered_map<std::string, MayaMeshShape>& meshes)
{
  std::vector<vm::vec3d> vertices;
  for (const auto& [meshName, mesh] : meshes)
  {
    (void)meshName;
    if (mesh.parentTransform != transformName)
    {
      continue;
    }
    for (const auto& local : mesh.localVertices)
    {
      const auto worldPt = world * vm::vec4d{local.x(), local.y(), local.z(), 1.0};
      const auto pt = vm::vec3d{worldPt.x(), worldPt.y(), worldPt.z()};
      const auto duplicate = std::any_of(
        vertices.begin(), vertices.end(), [&](const auto& existing) {
          return pointsNear(existing, pt);
        });
      if (!duplicate)
      {
        vertices.push_back(pt);
      }
    }
  }
  return vertices;
}

MayaBrushShapeMode brushShapeModeForNode(const MayaNode& node)
{
  if (const auto shape = stringAttr(node, "tb_brush_shape"))
  {
    if (*shape == "box" || *shape == "aabb")
    {
      return MayaBrushShapeMode::AxisAlignedBox;
    }
    if (*shape == "hull")
    {
      return MayaBrushShapeMode::ConvexHull;
    }
  }
  if (hasPrefix(node.name, "tb_brush_box_"))
  {
    return MayaBrushShapeMode::AxisAlignedBox;
  }
  if (hasPrefix(node.name, "tb_brush_hull_"))
  {
    return MayaBrushShapeMode::ConvexHull;
  }
  return MayaBrushShapeMode::Auto;
}

Result<Brush> brushForSpawn(
  const BrushBuilder& builder, const MayaAsciiEntitySpawn& spawn)
{
  const auto makeBox = [&]() -> Result<Brush> {
    if (!spawn.hasBrushBounds)
    {
      return Error{"Brush has no bounds"};
    }
    return builder.createCuboid(spawn.brushBounds, spawn.brushMaterial);
  };

  const auto makeHull = [&]() -> Result<Brush> {
    if (spawn.brushHullVertices.size() < 4)
    {
      return Error{"Brush hull needs at least four mesh vertices"};
    }
    return builder.createBrush(spawn.brushHullVertices, spawn.brushMaterial);
  };

  switch (spawn.brushShapeMode)
  {
  case MayaBrushShapeMode::AxisAlignedBox:
    return makeBox();
  case MayaBrushShapeMode::ConvexHull:
    if (auto hull = makeHull(); hull.is_success())
    {
      return hull;
    }
    return makeBox();
  case MayaBrushShapeMode::Auto:
    if (auto hull = makeHull(); hull.is_success())
    {
      return hull;
    }
    return makeBox();
  }
  return Error{"Unknown brush shape mode"};
}

} // namespace

bool canLoadMayaAsciiScene(const std::filesystem::path& path)
{
  return kdl::path_to_lower(path.extension()) == ".ma";
}

Result<std::vector<MayaAsciiEntitySpawn>> loadMayaAsciiScene(std::istream& stream)
{
  std::string line;
  if (!std::getline(stream, line))
  {
    return Error{"Maya ASCII file is empty"};
  }
  if (!startsWith(line, MayaAsciiMagic))
  {
    return Error{"Not a Maya ASCII scene (expected //Maya ASCII header)"};
  }

  std::unordered_map<std::string, MayaNode> nodes;
  std::unordered_map<std::string, MayaMeshShape> meshes;
  std::optional<std::string> currentNode;
  std::optional<std::string> currentMesh;
  bool parsingMeshVertices = false;

  auto ensureNode = [&](const std::string& name) -> MayaNode& {
    return nodes
      .emplace(
             name,
             MayaNode{name, std::nullopt, {0, 0, 0}, {0, 0, 0}, {1, 1, 1}, false, false, {}})
      .first->second;
  };

  while (std::getline(stream, line))
  {
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.starts_with("//"))
    {
      continue;
    }

    if (parsingMeshVertices)
    {
      if (
        trimmed.starts_with("setAttr ") || trimmed.starts_with("createNode ")
        || trimmed.starts_with("parent "))
      {
        parsingMeshVertices = false;
      }
      else if (looksLikeNumericContinuation(trimmed))
      {
        if (currentMesh)
        {
          appendFloat3Vertices(trimmed, meshes[*currentMesh].localVertices);
        }
        continue;
      }
      else
      {
        parsingMeshVertices = false;
      }
    }

    if (trimmed.starts_with("parent "))
    {
      currentNode = std::nullopt;
      currentMesh = std::nullopt;
      const auto quoted = parseQuotedStrings(trimmed);
      if (quoted.size() >= 2)
      {
        const auto& child = quoted[quoted.size() - 2];
        const auto& parent = quoted.back();
        auto childIt = nodes.find(child);
        if (childIt != nodes.end())
        {
          childIt->second.parent = parent;
        }
        auto meshIt = meshes.find(child);
        if (meshIt != meshes.end())
        {
          meshIt->second.parentTransform = parent;
        }
      }
      continue;
    }

    if (trimmed.starts_with("createNode "))
    {
      currentNode = std::nullopt;
      currentMesh = std::nullopt;

      if (trimmed.find("transform") != std::string::npos)
      {
        if (auto name = parseQuotedName(trimmed, "-n "))
        {
          auto& node = ensureNode(*name);
          if (auto parent = parseQuotedName(trimmed, "-p "))
          {
            node.parent = std::move(parent);
          }
          currentNode = name;
        }
      }
      else if (trimmed.find("locator") != std::string::npos)
      {
        if (auto name = parseQuotedName(trimmed, "-n "))
        {
          auto& node = ensureNode(*name);
          node.isLocator = true;
          if (auto parent = parseQuotedName(trimmed, "-p "))
          {
            node.parent = std::move(parent);
          }
          currentNode = name;
        }
      }
      else if (trimmed.find("mesh") != std::string::npos)
      {
        if (auto name = parseQuotedName(trimmed, "-n "))
        {
          auto& mesh = meshes.emplace(*name, MayaMeshShape{*name, std::nullopt, {}}).first->second;
          if (auto parent = parseQuotedName(trimmed, "-p "))
          {
            mesh.parentTransform = std::move(parent);
            auto parentIt = nodes.find(*parent);
            if (parentIt != nodes.end())
            {
              parentIt->second.hasMeshChild = true;
            }
          }
          currentMesh = name;
          currentNode = name;
        }
      }
      continue;
    }

    if (currentMesh && trimmed.starts_with("setAttr ") && trimmed.find(".vt[") != std::string::npos)
    {
      appendFloat3Vertices(trimmed, meshes[*currentMesh].localVertices);
      parsingMeshVertices = true;
      continue;
    }

    if (!currentNode)
    {
      continue;
    }

    auto nodeIt = nodes.find(*currentNode);
    if (nodeIt == nodes.end())
    {
      continue;
    }
    auto& node = nodeIt->second;

    if (trimmed.starts_with("setAttr "))
    {
      const auto attrQuote = trimmed.find('"');
      std::string attr;
      if (attrQuote != std::string::npos)
      {
        const auto attrEnd = trimmed.find('"', attrQuote + 1);
        if (attrEnd != std::string::npos)
        {
          attr = trimmed.substr(attrQuote + 1, attrEnd - attrQuote - 1);
        }
      }

      if (attr == ".t")
      {
        if (auto t = parseDouble3(trimmed))
        {
          node.translation = *t;
        }
      }
      else if (attr == ".r")
      {
        if (auto r = parseDouble3(trimmed))
        {
          node.rotationDeg = *r;
        }
      }
      else if (attr == ".s")
      {
        if (auto s = parseDouble3(trimmed))
        {
          node.scale = *s;
        }
      }
      else if (trimmed.find("-type \"string\"") != std::string::npos && attr.starts_with("."))
      {
        auto key = attr.substr(1);
        if (auto value = parseStringAttr(trimmed))
        {
          node.stringAttrs[std::move(key)] = std::move(*value);
        }
      }
    }
    else if (trimmed.starts_with("addAttr ") && trimmed.find("-ln \"") != std::string::npos)
    {
      if (auto attr = parseQuotedName(trimmed, "-ln \""))
      {
        node.stringAttrs.try_emplace(*attr, "");
      }
    }
  }

  std::unordered_map<std::string, vm::mat4x4d> worldCache;
  std::vector<MayaAsciiEntitySpawn> spawns;
  spawns.reserve(nodes.size());

  for (const auto& [name, node] : nodes)
  {
    (void)name;
    const auto role = resolveObjectRole(node);
    const auto classname = resolveClassname(node, role);
    if (!shouldImportNode(node, role, classname))
    {
      continue;
    }

    const auto world = worldTransform(node.name, nodes, worldCache);
    const auto origin = vm::vec3d{world[3][0], world[3][1], world[3][2]};

    MayaAsciiEntitySpawn spawn;
    spawn.kind = role == MayaObjectRole::BrushEntity ? MayaAsciiImportKind::Brush
                                                     : MayaAsciiImportKind::Point;
    spawn.classname = classname;
    spawn.origin = origin;
    spawn.angles = formatAngles(world);
    spawn.brushMaterial = brushMaterialForNode(node);

    for (const auto& [key, value] : node.stringAttrs)
    {
      if (
        key == "tb_class" || key == "tb_model" || key == "tb_kind" || key == "tb_material"
        || key == "tb_brush_shape")
      {
        continue;
      }
      if (!value.empty())
      {
        spawn.extraProperties.emplace(key, value);
      }
    }

    if (const auto model = modelPathForNode(node, role))
    {
      spawn.extraProperties.emplace("model", *model);
    }

    if (spawn.kind == MayaAsciiImportKind::Brush)
    {
      spawn.brushShapeMode = brushShapeModeForNode(node);
      spawn.brushHullVertices = meshWorldVertices(node.name, world, meshes);
      if (auto bounds = meshWorldBounds(node.name, world, meshes))
      {
        spawn.brushBounds = *bounds;
        spawn.hasBrushBounds = true;
      }
      else
      {
        spawn.brushBounds = defaultBrushBounds(origin);
        spawn.hasBrushBounds = true;
      }
    }

    spawns.push_back(std::move(spawn));
  }

  if (spawns.empty())
  {
    return Error{
      "No importable objects found. Tag transforms/locators with tb_entity_*, tb_spawn_*, "
      "tb_trigger_*, tb_brush_*, tb_model_* / tb_model, or tb_class / tb_kind."};
  }

  return spawns;
}

std::vector<Node*> importMayaAsciiSceneIntoMap(
  Map& map, const std::vector<MayaAsciiEntitySpawn>& spawns)
{
  if (spawns.empty())
  {
    return {};
  }

  BrushBuilder builder{map.worldNode().mapFormat(), map.worldBounds()};
  std::vector<Node*> created;
  created.reserve(spawns.size() * 2);

  for (const auto& spawn : spawns)
  {
    if (spawn.kind == MayaAsciiImportKind::Brush)
    {
      auto entity = Entity{};
      entity.setClassname(spawn.classname);
      entity.setOrigin(spawn.origin);
      if (!spawn.angles.empty())
      {
        entity.addOrUpdateProperty(EntityPropertyKeys::Angles, spawn.angles);
      }
      for (const auto& [key, value] : spawn.extraProperties)
      {
        entity.addOrUpdateProperty(key, value);
      }

      auto* entityNode = new EntityNode{std::move(entity)};
      created.push_back(entityNode);

      if (spawn.hasBrushBounds)
      {
        if (auto brush = brushForSpawn(builder, spawn); brush.is_success())
        {
          auto* brushNode = new BrushNode{brush.value()};
          created.push_back(brushNode);
          addNodes(map, {{parentForNodes(map), {entityNode}}});
          addNodes(map, {{entityNode, {brushNode}}});
          continue;
        }
      }

      addNodes(map, {{parentForNodes(map), {entityNode}}});
      continue;
    }

    auto entity = Entity{};
    entity.setClassname(spawn.classname);
    entity.setOrigin(spawn.origin);
    entity.addOrUpdateProperty(EntityPropertyKeys::Angles, spawn.angles);
    for (const auto& [key, value] : spawn.extraProperties)
    {
      entity.addOrUpdateProperty(key, value);
    }
    auto* entityNode = new EntityNode{std::move(entity)};
    created.push_back(entityNode);
    addNodes(map, {{parentForNodes(map), {entityNode}}});
  }

  return created;
}

} // namespace tb::mdl
