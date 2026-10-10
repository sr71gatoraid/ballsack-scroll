// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "autoscrollvisual.h"
#include "glyphstyle.h"
#include <scene/imageitem.h>
#include <scene/item.h>
#ifdef AUTOSCROLL_USE_LEGACY_IMAGE_ITEM_FACTORY
#include <scene/itemrenderer.h>
#include <scene/scene.h>
#endif
namespace AutoScroll {
namespace {
std::unique_ptr<KWin::ImageItem> createImageItem(KWin::Item *parent) {
#ifdef AUTOSCROLL_USE_LEGACY_IMAGE_ITEM_FACTORY
  return parent->scene()->renderer()->createImageItem(parent);
#else
  return std::make_unique<KWin::ImageItem>(parent);
#endif
}
}
AutoScrollVisual::AutoScrollVisual(KWin::Item *overlayItem)
    : m_overlayItem(overlayItem) {}
AutoScrollVisual::~AutoScrollVisual() = default;
void AutoScrollVisual::setAppearance(const QString &styleId, qreal glyphSize) {
  const QString style = normalizedGlyphStyleId(styleId);
  const qreal size = std::max(1.0, glyphSize);
  if (m_styleId == style && qFuzzyCompare(m_anchorSize, size)) return;
  m_styleId = style;
  m_anchorSize = size;
  if (m_anchorItem) {
    m_anchorItem->setImage(renderAnchor(m_scale));
    m_anchorItem->setSize(QSizeF(size, size));
    m_anchorItem->setPosition(m_anchorPosition - QPointF(size / 2, size / 2));
  }
}
void AutoScrollVisual::show(const QPointF &anchorPosition, qreal scale) {
  hide();
  m_anchorPosition = anchorPosition;
  m_scale = std::max(scale, 1.0);
  m_anchorItem = createImageItem(m_overlayItem);
  m_anchorItem->setImage(renderAnchor(m_scale));
  m_anchorItem->setSize(QSizeF(m_anchorSize, m_anchorSize));
  m_anchorItem->setPosition(anchorPosition - QPointF(m_anchorSize / 2, m_anchorSize / 2));
  m_anchorItem->setZ(1000);
}
void AutoScrollVisual::update(qreal scale) {
  const qreal boundedScale = std::max(scale, 1.0);
  if (m_anchorItem && !qFuzzyCompare(boundedScale, m_scale)) {
    m_scale = boundedScale;
    m_anchorItem->setImage(renderAnchor(m_scale));
  }
}
void AutoScrollVisual::hide() { m_anchorItem.reset(); }
bool AutoScrollVisual::isVisible() const { return m_anchorItem != nullptr; }
qreal AutoScrollVisual::logicalSize() const { return m_anchorSize; }
QImage AutoScrollVisual::renderAnchor(qreal scale) const {
  return renderGlyph(m_styleId, GlyphElement::Anchor, m_anchorSize, scale);
}
} // namespace AutoScroll
