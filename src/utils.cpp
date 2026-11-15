#include "utils.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <qactiongroup.h>

#include <unistd.h>

namespace {

// Quotes a value so that it survives being passed to a POSIX shell.
QString shellQuote(const QString &value)
{
    QString quoted = value;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    quoted.prepend(QLatin1Char('\''));
    quoted.append(QLatin1Char('\''));
    return quoted;
}

utils::CommandResult runProcess(const QString &program, const QStringList &arguments)
{
    utils::CommandResult result;
    QProcess process;

    process.start(program, arguments);
    if (!process.waitForStarted(-1) || !process.waitForFinished(-1)) {
        return result;
    }

    result.launched = true;
    result.exitCode = process.exitCode();
    result.standardOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.standardError = QString::fromUtf8(process.readAllStandardError());
    return result;
}

#if defined(Q_OS_MACOS)
// Runs a program with administrator privileges through AppleScript. The output is
// redirected to files by the helper script so that binary data and encoding are
// not mangled by the Apple Event string conversion.
utils::CommandResult runElevatedProcess(const QString &program, const QStringList &arguments)
{
    utils::CommandResult result;

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return result;
    }

    const QString scriptPath = tempDir.filePath(QStringLiteral("run.sh"));
    const QString outputPath = tempDir.filePath(QStringLiteral("stdout"));
    const QString errorPath = tempDir.filePath(QStringLiteral("stderr"));
    const QString statusPath = tempDir.filePath(QStringLiteral("status"));

    QString script = QStringLiteral("#!/bin/sh\n");
    script += shellQuote(program);
    for (const QString &argument : arguments) {
        script += QLatin1Char(' ') + shellQuote(argument);
    }
    script += QStringLiteral(" > %1 2> %2\n").arg(shellQuote(outputPath), shellQuote(errorPath));
    // "do shell script" raises an AppleScript error as soon as the command exits with a
    // non-zero status, and smartctl does that for harmless reasons all the time (its
    // status is a bitmask: 4 = one SMART command failed, 8 = the disk is failing, ...).
    // Record the real status and always report success to osascript.
    script += QStringLiteral("echo $? > %1\nexit 0\n").arg(shellQuote(statusPath));

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return result;
    }
    scriptFile.write(script.toUtf8());
    scriptFile.close();
    scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);

    QString appleScriptCommand = QStringLiteral("/bin/sh ") + scriptPath;
    appleScriptCommand.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    appleScriptCommand.replace(QLatin1Char('"'), QStringLiteral("\\\""));

    // AppleScript aborts events that do not answer within two minutes by default,
    // while smartctl can take longer on a sleeping or slow drive.
    const QString appleScript = QStringLiteral("with timeout of 300 seconds\n"
                                               "do shell script \"%1\" with administrator privileges\n"
                                               "end timeout").arg(appleScriptCommand);

    // osascript exits with a non-zero status and a "User canceled" message when the
    // authentication dialog is dismissed or the password is rejected.
    const utils::CommandResult osascriptResult = runProcess(QStringLiteral("/usr/bin/osascript"),
                                                            {QStringLiteral("-e"), appleScript});

    result.launched = osascriptResult.launched;
    result.standardError = osascriptResult.standardError;

    // The helper always exits 0, so the status file is what tells apart "the program
    // ran" from "the elevation never happened" (cancelled dialog, rejected password).
    QFile statusFile(statusPath);
    bool haveStatus = false;
    int recordedStatus = -1;
    if (statusFile.open(QIODevice::ReadOnly)) {
        bool ok = false;
        recordedStatus = statusFile.readAll().trimmed().toInt(&ok);
        haveStatus = ok;
    }

    result.exitCode = haveStatus ? recordedStatus : osascriptResult.exitCode;
    result.authorized = osascriptResult.launched && osascriptResult.exitCode == 0 && haveStatus;

    QFile outputFile(outputPath);
    if (outputFile.open(QIODevice::ReadOnly)) {
        result.standardOutput = QString::fromUtf8(outputFile.readAll());
    }
    QFile errorFile(errorPath);
    if (errorFile.open(QIODevice::ReadOnly)) {
        result.standardError += QString::fromUtf8(errorFile.readAll());
    }

    return result;
}
#endif

} // namespace

QString utils::clearButtonGroup(QButtonGroup* buttonGroup, QHBoxLayout* horizontalLayout, QSpacerItem* buttonStretch, QMenu* menuDisk)
{
    QString currentDeviceName = buttonGroup->checkedButton()->property("deviceName").toString();

    QList<QAbstractButton*> buttons = buttonGroup->buttons();
    for (QAbstractButton* button : std::as_const(buttons)) {
        buttonGroup->removeButton(button);
        delete button;
    }
    horizontalLayout->removeItem(buttonStretch);
    delete buttonStretch;

    // Dirty hack to remove only the radio buttons
    QList<QAction*> actions = menuDisk->actions();
    bool foundSeparator = false;

    for (int i = 0; i < actions.size(); ++i) {
        QAction* action = actions[i];
        if (foundSeparator) {
            menuDisk->removeAction(action);
        } else if (action->isSeparator()) {
            foundSeparator = true;
        }
    }

    return currentDeviceName;
}

QString utils::getSmartctlPath() {
    QStringList paths = QString::fromLocal8Bit(qgetenv("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);

    paths << QStringLiteral("/usr/local/sbin")
          << QStringLiteral("/usr/local/bin")
          << QStringLiteral("/usr/sbin")
          << QStringLiteral("/sbin");

#if defined(Q_OS_MACOS)
    // Applications started from Finder get a minimal environment, so the Homebrew
    // and MacPorts prefixes have to be probed explicitly.
    paths << QStringLiteral("/opt/homebrew/sbin")
          << QStringLiteral("/opt/homebrew/bin")
          << QStringLiteral("/opt/local/sbin")
          << QStringLiteral("/opt/local/bin");
#endif

    for (const QString &path : std::as_const(paths)) {
        QString absolutePath = QDir(path).absoluteFilePath("smartctl");
        if (QFile::exists(absolutePath) && QFileInfo(absolutePath).isExecutable()) {
            return absolutePath;
        }
    }

    return QString();
}

utils::CommandResult utils::runCommand(const QString &program, const QStringList &arguments)
{
    CommandResult result = runProcess(program, arguments);
    result.authorized = result.launched;
    return result;
}

utils::CommandResult utils::runCommandAsRoot(const QString &program, const QStringList &arguments)
{
    if (isRunningAsRoot()) {
        return runCommand(program, arguments);
    }

#if defined(Q_OS_MACOS)
    return runElevatedProcess(program, arguments);
#else
    QStringList elevatedArguments;
    elevatedArguments << program << arguments;
    CommandResult result = runProcess(QStringLiteral("pkexec"), elevatedArguments);
    // pkexec returns 126 when authorization fails and 127 when the program is missing.
    result.authorized = result.launched && result.exitCode != 126 && result.exitCode != 127;
    return result;
#endif
}

bool utils::isRunningAsRoot() const
{
    return geteuid() == 0;
}

QPair<QStringList, QJsonArray> utils::scanDevices(bool initializing)
{
    QString smartctlPath = getSmartctlPath();

    if (smartctlPath.isEmpty()) {
        QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("smartctl was not found, please install it!"));
        QTimer::singleShot(0, qApp, &QApplication::quit);
        return QPair<QStringList, QJsonArray>(QStringList(), QJsonArray());
    }

    CommandResult scanResult = runCommand(smartctlPath, {QStringLiteral("--scan"), QStringLiteral("--json")});
    QJsonDocument doc = QJsonDocument::fromJson(scanResult.standardOutput.toUtf8());
    QJsonObject jsonObj = doc.object();
    QJsonArray devices = jsonObj["devices"].toArray();

#if defined(Q_OS_MACOS)
    // Devices can be hidden from unprivileged processes. Only retry with elevated
    // rights when the plain scan found nothing, so the user is not prompted twice.
    if (devices.isEmpty()) {
        CommandResult elevatedScan = runCommandAsRoot(smartctlPath, {QStringLiteral("--scan"), QStringLiteral("--json")});
        if (elevatedScan.authorized) {
            QJsonArray elevatedDevices = QJsonDocument::fromJson(elevatedScan.standardOutput.toUtf8())
                                             .object().value("devices").toArray();
            if (!elevatedDevices.isEmpty()) {
                devices = elevatedDevices;
            }
        }
    }
#endif

    QStringList commandList;

    for (const QJsonValue &value : std::as_const(devices)) {
        QJsonObject device = value.toObject();
        QString deviceName = device["name"].toString();
        // The type reported by --scan (nvme, sat, ata, scsi, ...) has to be forwarded,
        // otherwise smartctl misdetects devices - this is the norm on macOS.
        QString deviceType = device["type"].toString();

        QString command = shellQuote(smartctlPath) + QStringLiteral(" -x --json=o ");
        if (!deviceType.isEmpty()) {
            command += QStringLiteral("-d ") + shellQuote(deviceType) + QLatin1Char(' ');
        }
        command += shellQuote(deviceName);
        commandList.append(command);
    }

    if (commandList.isEmpty()) {
        return QPair<QStringList, QJsonArray>(QStringList(), devices);
    }

    CommandResult devicesResult = runCommandAsRoot(QStringLiteral("/bin/sh"),
                                                   {QStringLiteral("-c"), commandList.join(QStringLiteral(" ; "))});

    if (!devicesResult.authorized) {
        QMessageBox messageBox;
        messageBox.setIcon(QMessageBox::Critical);
        messageBox.setWindowTitle(QObject::tr("QDiskInfo Error"));
        messageBox.setText(QObject::tr("QDiskInfo needs root access in order to read S.M.A.R.T. data!"));
        const QString details = devicesResult.standardError.trimmed();
        if (!details.isEmpty()) {
            // Keep the reason reachable instead of failing silently (cancelled dialog,
            // rejected password, AppleScript error, ...).
            messageBox.setDetailedText(details);
        }
        messageBox.exec();
        if (initializing) {
            QTimer::singleShot(0, qApp, &QApplication::quit);
        }
        return QPair<QStringList, QJsonArray>(QStringList(), QJsonArray());
    }

    QString allDevicesOutput = devicesResult.standardOutput;
    QStringList deviceOutputs;

    qsizetype startIndex = 0;
    qsizetype endIndex = 0;

    static const QRegularExpression regex("\\}\\n\\{");

    while ((endIndex = allDevicesOutput.indexOf(regex, static_cast<int>(startIndex))) != -1) {
        ++endIndex;
        QString jsonFragment = allDevicesOutput.mid(static_cast<int>(startIndex), static_cast<int>(endIndex - startIndex));
        deviceOutputs.append(jsonFragment);
        startIndex = endIndex;
    }

    if (startIndex < allDevicesOutput.size()) {
        QString jsonFragment = allDevicesOutput.mid(static_cast<int>(startIndex));
        deviceOutputs.append(jsonFragment);
    }

    if (!allDevicesOutput.isEmpty()) {
        return QPair<QStringList, QJsonArray>(deviceOutputs, devices);
    } else {
        return QPair<QStringList, QJsonArray>(QStringList(), QJsonArray());
    }
}

QString utils::initiateSelfTest(const QString &testType, const QString &deviceNode)
{
    CommandResult result = runCommandAsRoot(getSmartctlPath(),
                                            {QStringLiteral("--json=o"), QStringLiteral("-x"),
                                             QStringLiteral("-t"), testType, deviceNode});

    if (!result.authorized) {
        return QString();
    }
    return result.standardOutput;
}

void utils::cancelSelfTest(const QString &deviceNode)
{
    CommandResult result = runCommandAsRoot(getSmartctlPath(), {QStringLiteral("-X"), deviceNode});

    if (!result.authorized) {
        QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("QDiskInfo needs root access in order to abort a self-test!"));
    } else if (result.exitCode == QProcess::NormalExit) {
        QMessageBox::information(nullptr, QObject::tr("Test Requested"), QObject::tr("The self-test has been aborted"));
    } else {
        QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("Error: Something went wrong"));
    }
}

void utils::selfTestHandler(const QString &mode, const QString &name, const QString &minutes) {
    QString output = initiateSelfTest(mode, name);
    if (output.isEmpty()) {
        QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("QDiskInfo needs root access in order to request a self-test!"));
    } else {
        QJsonDocument testDoc = QJsonDocument::fromJson(output.toUtf8());
        QJsonObject testObj = testDoc.object();
        QJsonObject smartctlObj = testObj.value("smartctl").toObject();
        int exitStatus = smartctlObj.value("exit_status").toInt();

        QJsonArray outputArray = smartctlObj["output"].toArray();
        static const QRegularExpression regex("\\((\\d+%)\\s*(\\w+)\\)");

        QString percentage;
        for (const QJsonValue &value : outputArray) {
            QString line = value.toString();
            QRegularExpressionMatch match = regex.match(line);
            if (match.hasMatch()) {
                percentage = match.captured(0);
                break;
            }
        }
        percentage.replace("remaining", QObject::tr("remaining"));
        percentage.replace("completed", QObject::tr("completed"));

        if (exitStatus == 4) {
            QMessageBox msgBox;
            msgBox.setWindowTitle(QObject::tr("Test Already Running"));
            msgBox.setText(QObject::tr("A self-test is already being performed") + " " + percentage + "\n" + QObject::tr("You can press the Ok button in order to abort the test that is currently running"));
            msgBox.setIcon(QMessageBox::Warning);

            msgBox.addButton(QMessageBox::Cancel);
            QPushButton *abortButton = msgBox.addButton(QMessageBox::Ok);

            msgBox.exec();

            if (msgBox.clickedButton() == abortButton) {
                cancelSelfTest(name);
            }
        } else if (exitStatus == 0) {
            QString infoMessage = QObject::tr("A self-test has been requested successfully");
            if (minutes != "0") {
                infoMessage = infoMessage + "\n" + QObject::tr("It will be completed after") + " " + minutes + " " + QObject::tr("minutes");
            }
            QMessageBox::information(nullptr, QObject::tr("Test Requested"), infoMessage);
        } else {
            QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("Error: Something went wrong"));
        }
    }
}

QString utils::toTitleCase(const QString& sentence) {
    QString result;
    bool capitalizeNext = true;

    for (const QChar& c : sentence) {
        if (c.isLetter()) {
            if (capitalizeNext) {
                result += c.toUpper();
                capitalizeNext = false;
            } else {
                result += c.toLower();
            }
        } else {
            result += c;
            if (c == ' ') {
                capitalizeNext = true;
            }
        }
    }

    return result;
}
