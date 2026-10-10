// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QImage>
#include <QSizeF>
#include <QString>
#include <QStringView>

#include <array>

namespace AutoScroll {

enum class GlyphElement {
  Anchor,
  Direction,
};

struct GlyphStyle {
  const char *id;
  const char *anchorResource;
  const char *directionResource;
};

const std::array<GlyphStyle, 8> &glyphStyles();
const GlyphStyle &glyphStyle(QStringView id);
QString normalizedGlyphStyleId(QStringView id);

const std::array<int, 9> &glyphSizePresets();
int normalizedGlyphSize(int size);
qreal screenRelativeGlyphSize(int size, const QSizeF &screenSize);

QImage renderGlyph(QStringView styleId, GlyphElement element, qreal logicalSize,
                   qreal scale, qreal rotation = 0.0);

} // namespace AutoScroll
