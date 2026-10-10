// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <QPointF>
#include <QString>
#include <memory>
namespace KWin { class ImageItem; class Item; }
namespace AutoScroll {
// The only visual is the fixed anchor. The compositor and application retain
// full control of the real cursor, including its theme, shape and hotspot.
class AutoScrollVisual {
public:
  explicit AutoScrollVisual(KWin::Item *overlayItem);
  ~AutoScrollVisual();
  void setAppearance(const QString &styleId, qreal glyphSize);
  void show(const QPointF &anchorPosition, qreal scale);
  void update(qreal scale);
  void hide();
  bool isVisible() const;
  qreal logicalSize() const;
private:
  QImage renderAnchor(qreal scale) const;
  KWin::Item *m_overlayItem;
  std::unique_ptr<KWin::ImageItem> m_anchorItem;
  QPointF m_anchorPosition;
  QString m_styleId = QStringLiteral("user-anchor");
  qreal m_anchorSize = 30.0;
  qreal m_scale = 1.0;
};
} // namespace AutoScroll
