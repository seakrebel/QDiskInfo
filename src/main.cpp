#include "mainwindow.h"
#include "asciiview.h"

#include <QApplication>
#include <QFile>
#include <QTranslator>
#include <QLocale>
#include <iostream>
#include <ostream>
#include <unistd.h>

int main(int argc, char *argv[])
{
    bool isHeadless = false;
    QString devicePath;
    QString outputPath;

    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);

        if (argument == "--ascii-view") {
            isHeadless = true;
            if (i + 1 < argc) {
                devicePath = QString::fromLocal8Bit(argv[++i]);
            } else {
                std::cerr << "Error: Missing device path after --ascii-view" << std::endl;
                return 1;
            }
        } else if (argument == "--output") {
            if (i + 1 < argc) {
                outputPath = QString::fromLocal8Bit(argv[++i]);
            } else {
                std::cerr << "Error: Missing file path after --output" << std::endl;
                return 1;
            }
        }
    }

    if (isHeadless) {
        if (geteuid() != 0) {
            std::cerr << "Error: This action requires root privileges!" << std::endl;
            return 1;
        }

        AsciiView asciiview;
        QVector<unsigned char> driveData = asciiview.readSMARTData(devicePath);

        if (driveData.isEmpty()) {
            std::cerr << "Error: Could not read raw S.M.A.R.T. data from " << devicePath.toLocal8Bit().constData() << std::endl;
            return 2;
        }

        if (outputPath.isEmpty()) {
            std::cout.write(reinterpret_cast<const char*>(driveData.constData()), driveData.size());
            std::cout.flush();
        } else {
            QFile outputFile(outputPath);
            if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                std::cerr << "Error: Could not open " << outputPath.toLocal8Bit().constData() << " for writing" << std::endl;
                return 1;
            }
            outputFile.write(reinterpret_cast<const char*>(driveData.constData()), driveData.size());
            outputFile.close();
        }
        return 0;
    }

    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QDiskInfo"));
    QApplication::setApplicationDisplayName(QStringLiteral("QDiskInfo"));

    QTranslator translator;
    if (translator.load(QLocale(), QLatin1String("qdiskinfo"), QLatin1String("_"), QLatin1String(":/translations"))) {
        a.installTranslator(&translator);
    }

    MainWindow w;
    w.setWindowIcon(QIcon(":/icons/icon.svg"));
    w.show();
    return a.exec();
}
