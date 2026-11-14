#pragma once

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QMenu>
#include <QSpacerItem>
#include <QStringList>

class utils
{
public:
    struct CommandResult
    {
        bool launched = false;   // the process was started and finished
        bool authorized = false; // it ran (or would have run) with the requested privileges
        int exitCode = -1;
        QString standardOutput;
        QString standardError;
    };

    utils() = default;

    QString clearButtonGroup(QButtonGroup* buttonGroup, QHBoxLayout* horizontalLayout, QSpacerItem* buttonStretch, QMenu* menuDisk);
    QString getSmartctlPath();
    CommandResult runCommand(const QString &program, const QStringList &arguments);
    CommandResult runCommandAsRoot(const QString &program, const QStringList &arguments);
    bool isRunningAsRoot() const;
    QPair<QStringList, QJsonArray> scanDevices(bool initializing);
    QString initiateSelfTest(const QString &testType, const QString &deviceNode);
    void cancelSelfTest(const QString &deviceNode);
    void selfTestHandler(const QString &mode, const QString &name, const QString &minutes);
    QString toTitleCase(const QString& sentence);
};
