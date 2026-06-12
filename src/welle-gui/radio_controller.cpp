#include "radio_controller.h"
#include <QJniObject>
#include <QDebug>
#include <QSettings>
#include <stdexcept>
#include "input_factory.h"
#include "raw_file.h"
#include "rtl_tcp.h"

// Note: Add any other necessary includes from your original file here

CRadioController::CRadioController(QVariantMap& commandLineOptions, QObject *parent)
    : QObject(parent), commandLineOptions(commandLineOptions)
{
    // Initialize your timers and signals here
}

CRadioController::~CRadioController() { closeDevice(); }

void CRadioController::closeDevice()
{
    radioReceiver.reset();
    device.reset();
    emit deviceClosed();
}

void CRadioController::stop()
{
    if (device) device->stop();
    
    #ifdef Q_OS_ANDROID
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        QJniObject intent("android/content/Intent", "(Landroid/content/Context;Ljava/lang/Class;)V", activity.object(), QJniObject::fromString("io/welle/welle/RadioForegroundService").object());
        activity.callObjectMethod("startForegroundService", "(Landroid/content/Intent;)Landroid/content/ComponentName;", intent.object());
    }
    #endif
}

void CRadioController::startScan(void)
{
    #ifdef Q_OS_ANDROID
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        QJniObject intent("android/content/Intent", "(Landroid/content/Context;Ljava/lang/Class;)V", activity.object(), QJniObject::fromString("io/welle/welle/RadioForegroundService").object());
        activity.callObjectMethod("startForegroundService", "(Landroid/content/Intent;)Landroid/content/ComponentName;", intent.object());
    }
    #endif

    stop();
    // Proceed with scan logic...
}

void CRadioController::stopScan(void)
{
    isChannelScan = false;
    emit isChannelScanChanged(isChannelScan);
    emit scanStopped();
    stop();
}

void CRadioController::setAGC(bool isAGC)
{
    this->isAGC = isAGC;
    if (device) device->setAgc(isAGC);
    emit agcChanged(isAGC);
}

// Ensure all other functions are properly closed with }
