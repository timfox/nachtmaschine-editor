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

#include "mdl/CatchConfig.h"
#include "mdl/BrushNode.h"
#include "mdl/EntityNode.h"
#include "mdl/LoadMayaAsciiScene.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Transaction.h"

#include "fs/DiskIO.h"

#include "kd/result.h"

#include "vm/vec.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{

TEST_CASE("LoadMayaAsciiScene")
{
  const auto fixturePath =
    std::filesystem::current_path() / "fixture/test/mdl/LoadMayaAsciiScene/level_entities.ma";

  SECTION("canLoadMayaAsciiScene")
  {
    CHECK(canLoadMayaAsciiScene(fixturePath));
    CHECK_FALSE(canLoadMayaAsciiScene("scene.obj"));
  }

  SECTION("loadMayaAsciiScene")
  {
    const auto& spawns =
      fs::Disk::withInputStream(fixturePath, loadMayaAsciiScene) | kdl::value();
    REQUIRE(spawns.size() == 3);

    const auto player = std::find_if(spawns.begin(), spawns.end(), [](const auto& s) {
      return s.classname == "info_player_start";
    });
    REQUIRE(player != spawns.end());
    CHECK(player->origin == vm::vec3d{128, -256, 0});
    CHECK_FALSE(player->angles.empty());

    const auto light = std::find_if(spawns.begin(), spawns.end(), [](const auto& s) {
      return s.classname == "light";
    });
    REQUIRE(light != spawns.end());
    CHECK(light->origin == vm::vec3d{0, 0, 64});

    const auto model = std::find_if(spawns.begin(), spawns.end(), [](const auto& s) {
      return s.classname == "misc_model";
    });
    REQUIRE(model != spawns.end());
    CHECK(model->extraProperties.at("model") == "models/props/crate.lwo");
  }

  SECTION("importMayaAsciiSceneIntoMap")
  {
    auto fixture = MapFixture{};
    auto& map = fixture.create();

    const auto& spawns =
      fs::Disk::withInputStream(fixturePath, loadMayaAsciiScene) | kdl::value();

    auto transaction = Transaction{map, "Import Maya Scene"};
    const auto nodes = importMayaAsciiSceneIntoMap(map, spawns);
    REQUIRE(nodes.size() == 3);
    transaction.commit();

    const auto hasPlayer = std::any_of(nodes.begin(), nodes.end(), [](const Node* node) {
      return dynamic_cast<const EntityNode*>(node) != nullptr
             && dynamic_cast<const EntityNode*>(node)->entity().classname()
                  == "info_player_start";
    });
    CHECK(hasPlayer);
  }

  SECTION("roles: triggers, spawns, brush mesh")
  {
    const auto rolesPath =
      std::filesystem::current_path() / "fixture/test/mdl/LoadMayaAsciiScene/level_roles.ma";
    const auto& spawns = fs::Disk::withInputStream(rolesPath, loadMayaAsciiScene) | kdl::value();
    REQUIRE(spawns.size() == 3);

    const auto trigger = std::find_if(spawns.begin(), spawns.end(), [](const auto& s) {
      return s.classname == "trigger_once";
    });
    REQUIRE(trigger != spawns.end());
    CHECK(trigger->kind == MayaAsciiImportKind::Brush);
    CHECK(trigger->hasBrushBounds);

    const auto spawn = std::find_if(spawns.begin(), spawns.end(), [](const auto& s) {
      return s.classname == "info_player_deathmatch";
    });
    REQUIRE(spawn != spawns.end());
    CHECK(spawn->kind == MayaAsciiImportKind::Point);

    const auto brush = std::find_if(spawns.begin(), spawns.end(), [](const auto& s) {
      return s.classname == "func_detail";
    });
    REQUIRE(brush != spawns.end());
    CHECK(brush->kind == MayaAsciiImportKind::Brush);
    CHECK(brush->hasBrushBounds);
    const auto size = brush->brushBounds.size();
    CHECK(size.x() > 50.0);
  }

  SECTION("parent command hierarchy")
  {
    const auto hierarchyPath = std::filesystem::current_path()
                               / "fixture/test/mdl/LoadMayaAsciiScene/level_hierarchy.ma";
    const auto& spawns =
      fs::Disk::withInputStream(hierarchyPath, loadMayaAsciiScene) | kdl::value();
    REQUIRE(spawns.size() == 1);
    CHECK(spawns.front().classname == "info_player_start");
    // level_root at Maya (100,0,0), child local (0,0,50) → world Maya (100,0,50) → TB (100,-50,0)
    CHECK(spawns.front().origin == vm::vec3d{100, -50, 0});
  }

  SECTION("invalid header")
  {
    std::istringstream stream{"not maya\n"};
    CHECK(loadMayaAsciiScene(stream).is_error());
  }
}

} // namespace tb::mdl
