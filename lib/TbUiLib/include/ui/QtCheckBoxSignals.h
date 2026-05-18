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

#include <QCheckBox>

#include <QtGlobal>

namespace tb::ui
{
// QCheckBox::checkStateChanged was added in Qt 6.7; Ubuntu 22.04/24.04 ship Qt 6.4.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
inline constexpr auto CheckBoxStateChanged = &QCheckBox::checkStateChanged;
#else
inline constexpr auto CheckBoxStateChanged = &QCheckBox::stateChanged;
#endif
} // namespace tb::ui
