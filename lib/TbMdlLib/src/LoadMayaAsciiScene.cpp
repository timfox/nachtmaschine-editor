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

#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/EntityProperties.h"
#include "mdl/EntityRotation.h"
#include "mdl/Map.h"
#include "mdl/Map_Nodes.h"
#include "kd/k.h"
#include "kd/path_utils.h"

#include "vm/mat_ext.h"

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

struct MayaNode
{
  std::string name;
  std::optional<std::string> parent;
  vm::vec3d translation{0, 0, 0};
  vm::vec3d rotationDeg{0, 0, 0};
  bool hasMeshChild = false;
  bool isLocator = false;
  std::map<std::string, std::string> stringAttrs;
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
  // Maya Y-up → id/TrenchBroom Z-up: (x, y, z)_maya → (x, -z, y)_world
  return vm::vec3d{t.x(), -t.z(), t.y()};
}

vm::mat4x4d mayaLocalTransform(const vm::vec3d& translation, const vm::vec3d& rotationDeg)
{
  const auto t = mayaTranslationToWorld(translation);
  const auto rx = vm::to_radians(rotationDeg.x());
  const auto ry = vm::to_radians(rotationDeg.y());
  const auto rz = vm::to_radians(rotationDeg.z());
  const auto rMaya =
    vm::rotation_matrix(vm::vec3d{1, 0, 0}, rx) * vm::rotation_matrix(vm::vec3d{0, 1, 0}, ry)
    * vm::rotation_matrix(vm::vec3d{0, 0, 1}, rz);
  // Basis change for rotation: B * R * B^-1 with B mapping Maya axes to world axes
  auto B = vm::mat4x4d::identity();
  B[0] = vm::vec4d{1, 0, 0, 0};
  B[1] = vm::vec4d{0, 0, -1, 0};
  B[2] = vm::vec4d{0, 1, 0, 0};
  const auto rWorld = B * rMaya * vm::transpose(B);
  return vm::translation_matrix(t) * rWorld;
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
  const auto local = mayaLocalTransform(node.translation, node.rotationDeg);
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

std::string resolveClassname(const MayaNode& node)
{
  const auto classIt = node.stringAttrs.find("tb_class");
  if (classIt != node.stringAttrs.end() && !classIt->second.empty())
  {
    return classIt->second;
  }

  auto name = node.name;
  if (hasPrefix(name, "tb_entity_"))
  {
    return stripPrefix(name, "tb_entity_");
  }
  if (hasPrefix(name, "entity_"))
  {
    return stripPrefix(name, "entity_");
  }
  if (hasPrefix(name, "tb_"))
  {
    return stripPrefix(name, "tb_");
  }

  if (node.hasMeshChild || node.isLocator)
  {
    return "misc_model";
  }

  return {};
}

bool shouldImportNode(const MayaNode& node, const std::string& classname)
{
  if (classname.empty() || isIgnoredMayaNodeName(node.name))
  {
    return false;
  }

  if (hasPrefix(node.name, "tb_entity_") || hasPrefix(node.name, "entity_"))
  {
    return true;
  }
  if (node.stringAttrs.contains("tb_class"))
  {
    return true;
  }
  if (node.hasMeshChild)
  {
    return true;
  }
  if (node.isLocator && hasPrefix(node.name, "tb_"))
  {
    return true;
  }
  return false;
}

std::string formatAngles(const vm::mat4x4d& worldMatrix)
{
  const auto rot = vm::strip_translation(worldMatrix);
  const auto pyr = entityYawPitchRoll(vm::mat4x4d::identity(), rot);
  return kdl::str_to_string(pyr.x()) + " " + kdl::str_to_string(pyr.y()) + " "
         + kdl::str_to_string(pyr.z());
}

std::optional<std::string> modelPathForNode(const MayaNode& node)
{
  const auto modelIt = node.stringAttrs.find("tb_model");
  if (modelIt != node.stringAttrs.end() && !modelIt->second.empty())
  {
    return modelIt->second;
  }
  const auto modelIt2 = node.stringAttrs.find("model");
  if (modelIt2 != node.stringAttrs.end() && !modelIt2->second.empty())
  {
    return modelIt2->second;
  }
  if (node.hasMeshChild && !node.name.empty())
  {
    return node.name;
  }
  return std::nullopt;
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
  std::optional<std::string> currentNode;

  auto ensureNode = [&](const std::string& name) -> MayaNode& {
    return nodes.emplace(name, MayaNode{name, std::nullopt, {0, 0, 0}, {0, 0, 0}, false, false, {}})
      .first->second;
  };

  while (std::getline(stream, line))
  {
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.starts_with("//"))
    {
      continue;
    }

    if (trimmed.starts_with("parent "))
    {
      currentNode = std::nullopt;
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
      }
      continue;
    }

    if (trimmed.starts_with("createNode "))
    {
      currentNode = std::nullopt;

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
        if (auto parent = parseQuotedName(trimmed, "-p "))
        {
          auto parentIt = nodes.find(*parent);
          if (parentIt != nodes.end())
          {
            parentIt->second.hasMeshChild = true;
          }
        }
      }
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
    const auto classname = resolveClassname(node);
    if (!shouldImportNode(node, classname))
    {
      continue;
    }

    const auto world = worldTransform(node.name, nodes, worldCache);
    const auto origin = vm::vec3d{world[3][0], world[3][1], world[3][2]};

    MayaAsciiEntitySpawn spawn;
    spawn.classname = classname;
    spawn.origin = origin;
    spawn.angles = formatAngles(world);

    for (const auto& [key, value] : node.stringAttrs)
    {
      if (key == "tb_class" || key == "tb_model")
      {
        continue;
      }
      if (!value.empty())
      {
        spawn.extraProperties.emplace(key, value);
      }
    }

    if (const auto model = modelPathForNode(node))
    {
      spawn.extraProperties.emplace("model", *model);
    }

    spawns.push_back(std::move(spawn));
  }

  if (spawns.empty())
  {
    return Error{
      "No importable entities found. Mark transforms with tb_entity_<classname>, entity_<classname>, "
      "tb_class, mesh children, or tb_model."};
  }

  return spawns;
}

std::vector<EntityNode*> importMayaAsciiSceneIntoMap(
  Map& map, const std::vector<MayaAsciiEntitySpawn>& spawns)
{
  if (spawns.empty())
  {
    return {};
  }

  std::vector<Node*> entityNodes;
  entityNodes.reserve(spawns.size());

  for (const auto& spawn : spawns)
  {
    auto entity = Entity{};
    entity.setClassname(spawn.classname);
    entity.setOrigin(spawn.origin);
    entity.addOrUpdateProperty(EntityPropertyKeys::Angles, spawn.angles);
    for (const auto& [key, value] : spawn.extraProperties)
    {
      entity.addOrUpdateProperty(key, value);
    }
    entityNodes.push_back(new EntityNode{std::move(entity)});
  }

  addNodes(map, {{parentForNodes(map), entityNodes}});

  std::vector<EntityNode*> result;
  result.reserve(entityNodes.size());
  for (auto* node : entityNodes)
  {
    result.push_back(static_cast<EntityNode*>(node));
  }
  return result;
}

} // namespace tb::mdl
