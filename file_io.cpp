#include "file_io.h"

#include "loader.h"
#include "segmentation/segmentation.h"
#include "skeleton/skeletonizer.h"
#include "viewer.h"

#include <quazipfile.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>

#include <ctime>

QString annotationFileDefaultName() {
    // Generate a default file name based on date and time.
    auto currentTime = time(nullptr);
    auto localTime = localtime(&currentTime);
    return QString("annotation-%1%2%3T%4%5.000.k.zip")
            .arg(1900 + localTime->tm_year)
            //value, right aligned padded to width 2, base 10, filled with '0'
            .arg(localTime->tm_mon + 1, 2, 10, QLatin1Char('0'))
            .arg(localTime->tm_mday, 2, 10, QLatin1Char('0'))
            .arg(localTime->tm_hour, 2, 10, QLatin1Char('0'))
            .arg(localTime->tm_min, 2, 10, QLatin1Char('0'));
}

QString annotationFileDefaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/annotationFiles/" + annotationFileDefaultName();
}

void annotationFileLoad(const QString & filename, const QString & treeCmtOnMultiLoad, bool *isSuccess) {
    bool annotationSuccess = false;
    bool mergelistSuccess = false;
    QRegularExpression cubeRegEx(R"regex(.*mag(?P<mag>[0-9]*)x(?P<x>[0-9]*)y(?P<y>[0-9]*)z(?P<z>[0-9]*)((\.seg\.sz)|(\.segmentation\.snappy)))regex");
    QuaZip archive(filename);
    if (archive.open(QuaZip::mdUnzip)) {
        for (auto valid = archive.goToFirstFile(); valid; valid = archive.goToNextFile()) {
            QuaZipFile file(&archive);
            const auto & fileInside = archive.getCurrentFileName();
            const auto match = cubeRegEx.match(fileInside);
            if (match.hasMatch()) {
                file.open(QIODevice::ReadOnly);
                const auto cubeCoord = CoordOfCube(match.captured("x").toInt(), match.captured("y").toInt(), match.captured("z").toInt());
                Loader::Controller::singleton().snappyCacheSupplySnappy(cubeCoord, match.captured("mag").toInt(), file.readAll().toStdString());
            }
        }
        if (archive.setCurrentFile("mergelist.txt")) {
            QuaZipFile file(&archive);
            Segmentation::singleton().mergelistLoad(file);
            mergelistSuccess = true;
        }
        if (archive.setCurrentFile("microworker.txt")) {
            QuaZipFile file(&archive);
            Segmentation::singleton().jobLoad(file);
        }
        //load skeleton last as it may depend on a loaded segmentation
        if (archive.setCurrentFile("annotation.xml")) {
            QuaZipFile file(&archive);
            state->viewer->skeletonizer->loadXmlSkeleton(file, treeCmtOnMultiLoad);
            annotationSuccess = true;
        }
        state->viewer->loader_notify();
    } else {
        qDebug() << "opening" << filename << "for reading failed";
    }

    if (NULL != isSuccess) {
        *isSuccess = mergelistSuccess || annotationSuccess;
    }
}

void annotationFileSave(const QString & filename, bool *isSuccess) {
    bool allSuccess = true;
    QuaZip archive_write(filename);
    if (archive_write.open(QuaZip::mdCreate)) {
        auto zipCreateFile = [](QuaZipFile & file_write, const QString & name, const int level){
            auto fileinfo = QuaZipNewInfo(name);
            //without permissions set, some archive utilities will not grant any on extract
            fileinfo.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
            return file_write.open(QIODevice::WriteOnly, fileinfo, nullptr, 0, Z_DEFLATED, level);
        };
        QuaZipFile file_write(&archive_write);
        const bool open = zipCreateFile(file_write, "annotation.xml", 1);
        if (open) {
            state->viewer->skeletonizer->saveXmlSkeleton(file_write);
        } else {
            qDebug() << filename << "saving nml failed";
            allSuccess = false;
        }
        if (Segmentation::singleton().hasObjects()) {
            QuaZipFile file_write(&archive_write);
            const bool open = zipCreateFile(file_write, "mergelist.txt", 1);
            if (open) {
                Segmentation::singleton().mergelistSave(file_write);
            } else {
                qDebug() << filename << "saving mergelist failed";
                allSuccess = false;
            }
        }
        if (Session::singleton().annotationMode.testFlag(AnnotationMode::Mode_MergeSimple)) {
            QuaZipFile file_write(&archive_write);
            const bool open = zipCreateFile(file_write, "microworker.txt", 1);
            if (open) {
                Segmentation::singleton().jobSave(file_write);
            } else {
                qDebug() << filename << "saving segmentation job failed";
                allSuccess = false;
            }
        }
        QTime time;
        time.start();
        const auto & cubes = Loader::Controller::singleton().getAllModifiedCubes();
        for (std::size_t i = 0; i < cubes.size(); ++i) {
            const auto magName = QString("%1_mag%2x%3y%4z%5.seg.sz").arg(state->name).arg(QString::number(std::pow(2, i)));
            for (const auto & pair : cubes[i]) {
                QuaZipFile file_write(&archive_write);
                const auto cubeCoord = pair.first;
                const auto name = magName.arg(cubeCoord.x).arg(cubeCoord.y).arg(cubeCoord.z);
                const bool open = zipCreateFile(file_write, name, 1);
                if (open) {
                    file_write.write(pair.second.c_str(), pair.second.length());
                } else {
                    qDebug() << filename << "saving snappy cube failed";
                    allSuccess = false;
                }
            }
        }
        qDebug() << "save" << time.restart();
    } else {
        qDebug() << "opening" << filename << " for writing failed";
        allSuccess = false;
    }

    if (allSuccess) {
        Session::singleton().unsavedChanges = false;
    }

    if (NULL != isSuccess) {
        *isSuccess = allSuccess;
    }
}

void nmlExport(const QString & filename) {
    QFile file(filename);
    file.open(QIODevice::WriteOnly);
    state->viewer->skeletonizer->saveXmlSkeleton(file);
    file.close();
}

/** This is a replacement for the old updateFileName
    It decides if a skeleton file has a revision(case 1) or not(case2).
    if case1 the revision substring is extracted, incremented and will be replaced.
    if case2 an initial revision will be inserted.
    This method is actually only needed for the save or save as slots, if incrementFileName is selected
*/
void updateFileName(QString & fileName) {
    QRegExp versionRegEx = QRegExp("(\\.)([0-9]{3})(\\.)");
    if (versionRegEx.indexIn(fileName) != -1) {
        const auto versionIndex = versionRegEx.pos(2);//get second regex aka version without file extension
        const auto incrementedVersion = fileName.midRef(versionIndex, 3).toInt() + 1;//3 chars following the dot
        fileName.replace(versionIndex, 3, QString("%1").arg(incrementedVersion, 3, 10, QChar('0')));//pad with zeroes
    } else {
        QFileInfo info(fileName);
        fileName = info.dir().absolutePath() + "/" + info.baseName() + ".001." + info.completeSuffix();
    }
}

std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> loadLookupTable(const QString & path) {
    auto kill = [&path](){
        const auto msg = QObject::tr("Failed to open LUT file: »%1«").arg(path);
        qWarning() << msg;
        throw std::runtime_error(msg.toUtf8());
    };

    const std::size_t expectedBinaryLutSize = 256 * 3;//RGB
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> table;
    QFile overlayLutFile(path);
    if (overlayLutFile.open(QIODevice::ReadOnly)) {
        if (overlayLutFile.size() == expectedBinaryLutSize) {//imageJ binary LUT
            const auto buffer = overlayLutFile.readAll();
            table.resize(256);
            for (int i = 0; i < 256; ++i) {
                table[i] = std::make_tuple(static_cast<uint8_t>(buffer[i]), static_cast<uint8_t>(buffer[256 + i]), static_cast<uint8_t>(buffer[512 + i]));
            }
        } else {//json
            QJsonDocument json_conf = QJsonDocument::fromJson(overlayLutFile.readAll());
            auto jarray = json_conf.array();
            if (!json_conf.isArray() || jarray.size() != 256) {//dataset adjustment currently requires 256 values
                kill();
            }
            table.resize(jarray.size());
            for (int i = 0; i < jarray.size(); ++i) {
                table[i] = std::make_tuple(jarray[i].toArray()[0].toInt(), jarray[i].toArray()[1].toInt(), (jarray[i].toArray()[2].toInt()));
            }
        }
    } else {
        kill();
    }
    return table;
}
