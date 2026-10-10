// SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "glyphstyle.h"

#include <QFile>
#include <QSet>
#include <QSvgRenderer>
#include <QTest>

using namespace AutoScroll;

namespace {
bool hasVisiblePixel(const QImage &image) {
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(image.pixel(x, y)) != 0) {
        return true;
      }
    }
  }
  return false;
}
} // namespace

class GlyphStyleTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void registryIsComplete();
  void normalizesConfiguration();
  void rendersEveryStyleAndSize();
  void rendersEveryDirectionRotation();
  void userAnchorUsesVectorsAndTranslucentGreyBackground();
  void sizeStaysProportionalToScreen();
};

void GlyphStyleTest::registryIsComplete() {
  QSet<QString> ids;
  QCOMPARE(glyphStyles().size(), 8);
  for (const GlyphStyle &style : glyphStyles()) {
    const QString id = QString::fromLatin1(style.id);
    QVERIFY2(!ids.contains(id), qPrintable(id));
    ids.insert(id);
    QVERIFY2(QFile::exists(QString::fromLatin1(style.anchorResource)),
             style.anchorResource);
    QVERIFY2(QFile::exists(QString::fromLatin1(style.directionResource)),
             style.directionResource);
  }
}

void GlyphStyleTest::normalizesConfiguration() {
  QCOMPARE(normalizedGlyphStyleId(u"breeze"), QStringLiteral("breeze"));
  QCOMPARE(normalizedGlyphStyleId(u"breeze-dark"),
           QStringLiteral("breeze-dark"));
  QCOMPARE(normalizedGlyphStyleId(u"pulse"), QStringLiteral("pulse"));
  QCOMPARE(normalizedGlyphStyleId(u"not-installed"),
           QStringLiteral("breeze-dark"));
  QCOMPARE(normalizedGlyphSize(-1), 16);
  QCOMPARE(normalizedGlyphSize(20), 16);
  QCOMPARE(normalizedGlyphSize(28), 30);
  QCOMPARE(normalizedGlyphSize(30), 30);
  QCOMPARE(normalizedGlyphSize(52), 48);
  QCOMPARE(normalizedGlyphSize(500), 72);
}

void GlyphStyleTest::rendersEveryStyleAndSize() {
  for (const GlyphStyle &style : glyphStyles()) {
    const QString id = QString::fromLatin1(style.id);
    for (const int size : glyphSizePresets()) {
      for (const qreal scale : {1.0, 2.0}) {
        const QImage anchor =
            renderGlyph(id, GlyphElement::Anchor, size, scale);
        const QImage direction =
            renderGlyph(id, GlyphElement::Direction, size * 0.8, scale);
        QVERIFY2(!anchor.isNull(), qPrintable(id));
        QVERIFY2(!direction.isNull(), qPrintable(id));
        QVERIFY2(hasVisiblePixel(anchor), qPrintable(id));
        QVERIFY2(hasVisiblePixel(direction), qPrintable(id));
        QCOMPARE(anchor.devicePixelRatio(), scale);
        QCOMPARE(direction.devicePixelRatio(), scale);
      }
    }
  }
}

void GlyphStyleTest::rendersEveryDirectionRotation() {
  for (int rotation = 0; rotation < 360; rotation += 45) {
    const QImage image = renderGlyph(u"breeze-dark", GlyphElement::Direction,
                                     32.0, 1.0, rotation);
    QVERIFY(hasVisiblePixel(image));
  }
}

void GlyphStyleTest::userAnchorUsesVectorsAndTranslucentGreyBackground() {
  const auto &style = glyphStyle(u"user-anchor");
  QVERIFY(QString::fromLatin1(style.anchorResource).endsWith(QStringLiteral(".svg")));
  QFile asset(QString::fromLatin1(style.anchorResource));
  QVERIFY(asset.open(QIODevice::ReadOnly));
  const QByteArray svg = asset.readAll();
  QSvgRenderer renderer(svg);
  QVERIFY(renderer.isValid());
  QVERIFY(!svg.contains("<image"));
  const QImage anchor = renderGlyph(u"user-anchor", GlyphElement::Anchor, 40.0, 1.0);
  QCOMPARE(qAlpha(anchor.pixel(0, 0)), 0);
  const QColor background = anchor.pixelColor(10, 10);
  QVERIFY(background.alpha() >= 126 && background.alpha() <= 129);
  QVERIFY(background.red() > 180 && background.red() < 250);
  QVERIFY(qAlpha(anchor.pixel(20, 20)) > 250);
}

void GlyphStyleTest::sizeStaysProportionalToScreen() {
  QCOMPARE(screenRelativeGlyphSize(30, QSizeF(2560, 1440)), 30.0);
  QCOMPARE(screenRelativeGlyphSize(30, QSizeF(1920, 1080)), 22.5);
  QCOMPARE(screenRelativeGlyphSize(30, QSizeF(3840, 2160)), 45.0);
  QCOMPARE(screenRelativeGlyphSize(30, QSizeF(1440, 2560)), 30.0);
  // The 4K output at 200% scaling has 1920x1080 logical geometry.
  QCOMPARE(screenRelativeGlyphSize(30, QSizeF(1920, 1080)) * 2.0, 45.0);
  QCOMPARE(screenRelativeGlyphSize(30, QSizeF()), 30.0);
}

QTEST_MAIN(GlyphStyleTest)

#include "glyphstyle_test.moc"
