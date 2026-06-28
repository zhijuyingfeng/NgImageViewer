#include "imagesequence.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class ImageSequenceTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyAndUnsupportedDirectoriesHaveNoNeighbors();
    void navigationSkipsUnsupportedFiles();
    void deleteNeighborSelectionKeepsBrowsingPosition();

private:
    static QString touch(QTemporaryDir &dir, const QString &fileName);
};

QString ImageSequenceTest::touch(QTemporaryDir &dir, const QString &fileName)
{
    const QString path = QDir(dir.path()).filePath(fileName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write("x");
    return path;
}

void ImageSequenceTest::emptyAndUnsupportedDirectoriesHaveNoNeighbors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString textPath = touch(dir, QStringLiteral("notes.txt"));
    QVERIFY(!textPath.isEmpty());

    ImageSequence sequence;
    sequence.rebuild(textPath);

    QVERIFY(!sequence.hasPrevious());
    QVERIFY(!sequence.hasNext());
    QVERIFY(sequence.previousPath().isEmpty());
    QVERIFY(sequence.nextPath().isEmpty());
}

void ImageSequenceTest::navigationSkipsUnsupportedFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString first = touch(dir, QStringLiteral("001.jpg"));
    QVERIFY(!first.isEmpty());
    QVERIFY(!touch(dir, QStringLiteral("002.txt")).isEmpty());
    const QString second = touch(dir, QStringLiteral("003.png"));
    QVERIFY(!second.isEmpty());
    const QString third = touch(dir, QStringLiteral("004.RW2"));
    QVERIFY(!third.isEmpty());

    ImageSequence sequence;
    sequence.rebuild(second);

    QVERIFY(sequence.hasPrevious());
    QVERIFY(sequence.hasNext());
    QCOMPARE(sequence.previousPath(), first);
    QCOMPARE(sequence.nextPath(), third);
}

void ImageSequenceTest::deleteNeighborSelectionKeepsBrowsingPosition()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!touch(dir, QStringLiteral("001.jpg")).isEmpty());
    const QString current = touch(dir, QStringLiteral("002.png"));
    QVERIFY(!current.isEmpty());
    const QString next = touch(dir, QStringLiteral("003.webp"));
    QVERIFY(!next.isEmpty());

    ImageSequence sequence;
    sequence.rebuild(current);
    QVERIFY(QFile::remove(current));

    QCOMPARE(sequence.nextPathAfterRemovingCurrent(current), next);
    QVERIFY(!sequence.hasNext());
    QVERIFY(sequence.hasPrevious());
}

QTEST_GUILESS_MAIN(ImageSequenceTest)
#include "test_imagesequence.moc"
