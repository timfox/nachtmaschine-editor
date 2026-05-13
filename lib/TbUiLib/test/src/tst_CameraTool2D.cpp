/*
 Copyright (C) 2026 Kristian Duske

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

#include "PreferenceManager.h"
#include "Preferences.h"
#include "gl/OrthographicCamera.h"
#include "ui/CameraTool2D.h"
#include "ui/CatchConfig.h"
#include "ui/InputState.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{

TEST_CASE("CameraTool2D")
{
  auto& prefs = PreferenceManager::instance();
  prefs.set(Preferences::CameraNavigationScheme, Preferences::CameraNavigationScheme_TrenchBroom);
  prefs.set(Preferences::CameraEnableAltMove, false);

  auto camera = gl::OrthographicCamera{};
  auto cameraTool = CameraTool2D{camera};
  auto inputState = InputState{};

  SECTION("Maya + Alt+middle pans when Alt+middle zoom mode is enabled")
  {
    prefs.set(Preferences::CameraNavigationScheme, Preferences::CameraNavigationScheme_Maya);
    prefs.set(Preferences::CameraEnableAltMove, true);

    inputState.setModifierKeys(ModifierKeys::Alt);
    inputState.mouseDown(MouseButtons::Middle);
    inputState.mouseMove(10, 0, 10, 0);

    CHECK(cameraTool.acceptMouseDrag(inputState) != nullptr);
  }

  SECTION("Maya + middle alone does not pan when Alt+middle zoom mode is enabled")
  {
    prefs.set(Preferences::CameraNavigationScheme, Preferences::CameraNavigationScheme_Maya);
    prefs.set(Preferences::CameraEnableAltMove, true);

    inputState.mouseDown(MouseButtons::Middle);
    inputState.mouseMove(10, 0, 10, 0);

    CHECK(cameraTool.acceptMouseDrag(inputState) == nullptr);
  }

  SECTION("Maya + Alt+right yields zoom drag (not blocked by pan)")
  {
    prefs.set(Preferences::CameraNavigationScheme, Preferences::CameraNavigationScheme_Maya);
    prefs.set(Preferences::CameraEnableAltMove, true);

    inputState.setModifierKeys(ModifierKeys::Alt);
    inputState.mouseDown(MouseButtons::Right);
    inputState.mouseMove(0, 50, 0, 50);

    CHECK(cameraTool.acceptMouseDrag(inputState) != nullptr);
  }

  SECTION("Maya + right without Alt still pans")
  {
    prefs.set(Preferences::CameraNavigationScheme, Preferences::CameraNavigationScheme_Maya);
    prefs.set(Preferences::CameraEnableAltMove, true);

    inputState.mouseDown(MouseButtons::Right);
    inputState.mouseMove(10, 0, 10, 0);

    CHECK(cameraTool.acceptMouseDrag(inputState) != nullptr);
  }
}

} // namespace tb::ui
