#include "imageloadstate.h"

#include <QTest>

class ImageLoadStateTest : public QObject
{
    Q_OBJECT

private slots:
    void beginLoadingTracksPendingRequest();
    void completeLoadingStoresCurrentDocument();
    void failedLoadingKeepsExistingDocument();
    void failedLoadingWithoutCurrentReturnsToEmptyState();
    void clearCurrentReturnsToEmptyState();
};

void ImageLoadStateTest::beginLoadingTracksPendingRequest()
{
    ImageLoadState state;

    const quint64 firstRequest = state.beginLoading(QStringLiteral("/tmp/first.jpg"));
    const quint64 secondRequest = state.beginLoading(QStringLiteral("/tmp/second.jpg"));

    QVERIFY(firstRequest != secondRequest);
    QVERIFY(!state.isActiveRequest(firstRequest));
    QVERIFY(state.isActiveRequest(secondRequest));
    QVERIFY(state.isLoading());
    QCOMPARE(state.pendingPath(), QStringLiteral("/tmp/second.jpg"));
    QCOMPARE(state.viewerState(), ImageLoadState::ViewerState::Loading);
}

void ImageLoadStateTest::completeLoadingStoresCurrentDocument()
{
    ImageLoadState state;
    state.beginLoading(QStringLiteral("/tmp/image.jpg"));

    ImageDocument document;
    document.filePath = QStringLiteral("/tmp/image.jpg");
    document.kind = ImageLoader::Kind::StaticImage;
    document.displayedSize = QSize(640, 480);
    state.completeLoading(document);

    QVERIFY(!state.isLoading());
    QVERIFY(state.hasCurrent());
    QVERIFY(state.current());
    QCOMPARE(state.current()->filePath, document.filePath);
    QCOMPARE(state.current()->displayedSize, document.displayedSize);
    QCOMPARE(state.currentPath(), document.filePath);
    QCOMPARE(state.pendingPath(), QString());
    QCOMPARE(state.viewerState(), ImageLoadState::ViewerState::ShowingImage);
}

void ImageLoadStateTest::failedLoadingKeepsExistingDocument()
{
    ImageLoadState state;

    ImageDocument document;
    document.filePath = QStringLiteral("/tmp/existing.jpg");
    document.kind = ImageLoader::Kind::StaticImage;
    state.completeLoading(document);

    state.beginLoading(QStringLiteral("/tmp/missing.jpg"));
    state.failLoading();

    QVERIFY(!state.isLoading());
    QVERIFY(state.hasCurrent());
    QCOMPARE(state.currentPath(), document.filePath);
    QCOMPARE(state.pendingPath(), QString());
    QCOMPARE(state.viewerState(), ImageLoadState::ViewerState::ShowingImage);
}

void ImageLoadStateTest::failedLoadingWithoutCurrentReturnsToEmptyState()
{
    ImageLoadState state;

    state.beginLoading(QStringLiteral("/tmp/missing.jpg"));
    state.failLoading();

    QVERIFY(!state.isLoading());
    QVERIFY(!state.hasCurrent());
    QCOMPARE(state.currentPath(), QString());
    QCOMPARE(state.pendingPath(), QString());
    QCOMPARE(state.viewerState(), ImageLoadState::ViewerState::Empty);
}

void ImageLoadStateTest::clearCurrentReturnsToEmptyState()
{
    ImageLoadState state;

    ImageDocument document;
    document.filePath = QStringLiteral("/tmp/image.jpg");
    document.kind = ImageLoader::Kind::StaticImage;
    state.completeLoading(document);

    state.clearCurrent();

    QVERIFY(!state.isLoading());
    QVERIFY(!state.hasCurrent());
    QCOMPARE(state.currentPath(), QString());
    QCOMPARE(state.viewerState(), ImageLoadState::ViewerState::Empty);
    QVERIFY(!state.sequence().hasPrevious());
    QVERIFY(!state.sequence().hasNext());
}

QTEST_GUILESS_MAIN(ImageLoadStateTest)
#include "test_imageloadstate.moc"
