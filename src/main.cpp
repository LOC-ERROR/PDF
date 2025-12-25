#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kDefaultDpi = 150;
constexpr char kOutputFilename[] = "images.pdf";
const QStringList kImageFilters = {
    "*.jpg", "*.jpeg", "*.png", "*.tif", "*.tiff", "*.bmp", "*.webp",
};

QPageSize pageSizeForImage(const QImage &image, int dpi) {
    const double widthPoints = image.width() * 72.0 / dpi;
    const double heightPoints = image.height() * 72.0 / dpi;
    return QPageSize(QSizeF(widthPoints, heightPoints), QPageSize::Point);
}
}  // namespace

class ConverterWorker : public QObject {
    Q_OBJECT

public:
    explicit ConverterWorker(QString folderPath, QObject *parent = nullptr)
        : QObject(parent), folderPath_(std::move(folderPath)) {}

signals:
    void progress(int value);
    void status(const QString &message);
    void finished(const QString &outputPath);
    void failed(const QString &message);

public slots:
    void process() {
        emit status("Scanning folder...");

        QDir dir(folderPath_);
        if (!dir.exists()) {
            emit failed("The provided folder does not exist.");
            return;
        }

        QStringList files = dir.entryList(
            kImageFilters, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        if (files.isEmpty()) {
            emit failed("No images found in the selected folder.");
            return;
        }

        QList<QString> imagePaths;
        imagePaths.reserve(files.size());
        for (const QString &file : files) {
            imagePaths.append(dir.absoluteFilePath(file));
        }

        const QString outputPath = dir.absoluteFilePath(kOutputFilename);
        emit status("Creating PDF...");

        QImage firstImage(imagePaths.first());
        if (firstImage.isNull()) {
            emit failed("The first image could not be loaded.");
            return;
        }

        QPdfWriter pdf(outputPath);
        pdf.setResolution(kDefaultDpi);
        pdf.setPageSize(pageSizeForImage(firstImage, kDefaultDpi));

        QPainter painter(&pdf);
        if (!painter.isActive()) {
            emit failed("Failed to initialize PDF writer.");
            return;
        }

        for (int index = 0; index < imagePaths.size(); ++index) {
            QImage image(imagePaths.at(index));
            if (image.isNull()) {
                emit failed(QString("Failed to load image: %1").arg(imagePaths.at(index)));
                return;
            }

            if (index > 0) {
                pdf.setPageSize(pageSizeForImage(image, kDefaultDpi));
                pdf.newPage();
            }

            QRect targetRect(QPoint(0, 0), pdf.pageLayout().fullRectPixels(pdf.resolution()).size());
            painter.drawImage(targetRect, image);

            const int percent = static_cast<int>((index + 1) * 100.0 / imagePaths.size());
            emit progress(percent);
        }

        painter.end();
        emit finished(outputPath);
    }

private:
    QString folderPath_;
};

class MainWindow : public QWidget {
    Q_OBJECT

public:
    MainWindow() {
        setWindowTitle("Image Folder to PDF");
        setFixedSize(680, 360);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(16);

        auto *header = new QLabel("Convert all images in a folder to a PDF");
        QFont headerFont = header->font();
        headerFont.setPointSize(14);
        headerFont.setBold(true);
        header->setFont(headerFont);
        layout->addWidget(header);

        auto *description = new QLabel(
            "Select or enter a folder path. Images are embedded at 100% resolution and "
            "exported at 150 DPI.");
        description->setWordWrap(true);
        layout->addWidget(description);

        auto *pathLayout = new QHBoxLayout();
        auto *pathLabel = new QLabel("Folder path:");
        pathEdit_ = new QLineEdit();
        auto *browseButton = new QPushButton("Browse...");
        connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseFolder);
        pathLayout->addWidget(pathLabel);
        pathLayout->addWidget(pathEdit_, 1);
        pathLayout->addWidget(browseButton);
        layout->addLayout(pathLayout);

        auto *actionLayout = new QHBoxLayout();
        convertButton_ = new QPushButton("Convert to PDF");
        connect(convertButton_, &QPushButton::clicked, this, &MainWindow::startConversion);
        progressBar_ = new QProgressBar();
        progressBar_->setRange(0, 100);
        progressBar_->setValue(0);
        actionLayout->addWidget(convertButton_);
        actionLayout->addWidget(progressBar_, 1);
        layout->addLayout(actionLayout);

        statusLabel_ = new QLabel("Ready");
        statusLabel_->setStyleSheet("color: #555;");
        layout->addWidget(statusLabel_);

        auto *info = new QLabel(
            "• Output PDF name: images.pdf\n"
            "• Output location: selected folder\n"
            "• Image order: alphabetical by filename");
        info->setStyleSheet("background: #f6f6f6; padding: 12px; border-radius: 6px;");
        layout->addWidget(info);
    }

private slots:
    void browseFolder() {
        const QString folder = QFileDialog::getExistingDirectory(
            this, "Select a folder with images");
        if (!folder.isEmpty()) {
            pathEdit_->setText(folder);
        }
    }

    void startConversion() {
        const QString folder = pathEdit_->text().trimmed();
        if (folder.isEmpty()) {
            QMessageBox::warning(this, "Missing folder", "Please provide a folder path.");
            return;
        }

        QDir dir(folder);
        if (!dir.exists()) {
            QMessageBox::critical(this, "Invalid folder", "The provided path is not a folder.");
            return;
        }

        convertButton_->setEnabled(false);
        progressBar_->setValue(0);
        statusLabel_->setText("Preparing conversion...");

        auto *worker = new ConverterWorker(folder);
        auto *thread = new QThread(this);
        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &ConverterWorker::process);
        connect(worker, &ConverterWorker::progress, progressBar_, &QProgressBar::setValue);
        connect(worker, &ConverterWorker::status, statusLabel_, &QLabel::setText);
        connect(worker, &ConverterWorker::finished, this, &MainWindow::onFinished);
        connect(worker, &ConverterWorker::failed, this, &MainWindow::onFailed);
        connect(worker, &ConverterWorker::finished, thread, &QThread::quit);
        connect(worker, &ConverterWorker::failed, thread, &QThread::quit);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }

    void onFinished(const QString &outputPath) {
        statusLabel_->setText(QString("Done! Saved to %1").arg(outputPath));
        convertButton_->setEnabled(true);
    }

    void onFailed(const QString &message) {
        QMessageBox::critical(this, "Conversion error", message);
        statusLabel_->setText("Ready");
        progressBar_->setValue(0);
        convertButton_->setEnabled(true);
    }

private:
    QLineEdit *pathEdit_{nullptr};
    QPushButton *convertButton_{nullptr};
    QProgressBar *progressBar_{nullptr};
    QLabel *statusLabel_{nullptr};
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
