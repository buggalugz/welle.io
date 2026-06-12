/*
 * Copyright (C) 2018
 * Matthias P. Braendli (matthias.braendli@mpb.li)
 *
 * Copyright (C) 2017
 * Albrecht Lohofener (albrechtloh@gmx.de)
 *
 * This file is based on SDR-J
 * Copyright (C) 2010, 2011, 2012
 * Jan van Katwijk (J.vanKatwijk@gmail.com)
 *
 * This file is part of the welle.io.
 */

#include <QJniObject>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>
#include <QTimeZone>
#include <stdexcept>

#include "radio_controller.h"
#ifdef HAVE_SOAPYSDR
#include "soapy_sdr.h"
#endif /* HAVE_SOAPYSDR */
#include "input_factory.h"
#include "raw_file.h"
#include "rtl_tcp.h"

#define AUDIOBUFFERSIZE 32768

static QString serialise_serviceid(quint32 serviceid) {
    return QString::asprintf("%x", serviceid);
}

static quint32 deserialise_serviceid(const char *input)
{
    long value = 0;
    errno = 0;

    char* endptr = nullptr;
    value = strtol(input, &endptr, 16);

    if ((value == LONG_MIN or value == LONG_MAX) and errno == ERANGE) {
        return 0;
    }
    else if (value == 0 and errno != 0) {
        return 0;
    }
    else if (input == endptr) {
        return 0;
    }
    else if (*endptr != '\0') {
        return 0;
    }

    return value;
}

CRadioController::CRadioController(QVariantMap& commandLineOptions, QObject *parent)
    : QObject(parent)
    , commandLineOptions(commandLineOptions)
    , audioBuffer(2 * AUDIOBUFFERSIZE)
    , audio(audioBuffer)
{
    resetTechnicalData();

    connect(&labelTimer, &QTimer::timeout, this, &CRadioController::labelTimerTimeout);
    connect(&stationTimer, &QTimer::timeout, this, &CRadioController::stationTimerTimeout);
    connect(&channelTimer, &QTimer::timeout, this, &CRadioController::channelTimerTimeout);

    connect(this, &CRadioController::switchToNextChannel,
            this, &CRadioController::nextChannel);

    connect(this, &CRadioController::ensembleIdUpdated,
            this, &CRadioController::ensembleId);

    qRegisterMetaType<DabLabel>("DabLabel&");
    connect(this, &CRadioController::ensembleLabelUpdated,
            this, &CRadioController::ensembleLabel);
        
    connect(this, &CRadioController::serviceDetected,
            this, &CRadioController::serviceId);

    qRegisterMetaType<dab_date_time_t>("dab_date_time_t");
    connect(this, &CRadioController::dateTimeUpdated,
            this, &CRadioController::displayDateTime);

    connect(this, &CRadioController::restartServiceRequested,
            this, &CRadioController::restartService);
}

CRadioController::~CRadioController()
{
    closeDevice();
    qDebug() << "RadioController:" << "Deleting CRadioController";
}

void CRadioController::closeDevice()
{
    qDebug() << "RadioController:" << "Close device";
    radioReceiver.reset();
    device.reset();
    audio.reset();
    resetTechnicalData();
    emit deviceClosed();
}

CDeviceID CRadioController::openDevice(CDeviceID deviceId, bool force, QVariant param1, QVariant param2)
{
    if(this->deviceId != deviceId || force) {
        closeDevice();
        device.reset(CInputFactory::GetDevice(*this, deviceId));

        if (device->getID() == CDeviceID::RTL_TCP) {
            CRTL_TCP_Client* RTL_TCP_Client = static_cast<CRTL_TCP_Client*>(device.get());
            RTL_TCP_Client->setServerAddress(param1.toString().toStdString());
            RTL_TCP_Client->setPort(param2.toInt());
        }

        if (device->getID() == CDeviceID::RAWFILE) {
            CRAWFile* rawFile = static_cast<CRAWFile*>(device.get());
#ifdef __ANDROID__
            rawFileAndroid.reset(new QFile(param1.toString()));
            bool ret = rawFileAndroid->open(QIODevice::ReadOnly);
            if(!ret) {
                setErrorMessage(tr("Error while opening file ") + param1.toString());
            }
            else {
                rawFile->setFileHandle(rawFileAndroid->handle(), param2.toString().toStdString());
            }
#else
            rawFile->setFileName(param1.toString().toStdString(), param2.toString().toStdString());
#endif
        }
        initialise();
    }
    return device->getID();
}


CDeviceID CRadioController::openDevice()
{
    closeDevice();
    device.reset(CInputFactory::GetDevice(*this, "auto"));
    initialise();
    return device->getID();
}

void CRadioController::setDeviceParam(QString param, int value)
{
    if (param == "biastee") {
        deviceParametersInt[DeviceParam::BiasTee] = value;
    } else {
        qDebug() << "Invalid device parameter setting: " << param;
    }

    if (device) {
        device->setDeviceParam(DeviceParam::BiasTee, value);
    }
}

void CRadioController::setDeviceParam(QString param, QString value)
{
    DeviceParam dp;
    bool deviceParamChanged = false;

    if (param == "SoapySDRAntenna") {
        dp = DeviceParam::SoapySDRAntenna;
    } else if (param == "SoapySDRDriverArgs") {
        dp = DeviceParam::SoapySDRDriverArgs;
    } else if (param == "SoapySDRClockSource") {
        dp = DeviceParam::SoapySDRClockSource;
    } else {
        qDebug() << "Invalid device parameter setting: " << param;
        return;
    }

    std::string v = value.toStdString();
    if (deviceParametersString[dp] != v) {
        deviceParamChanged = true;
        deviceParametersString[dp] = v;
    }

    if (device && deviceParamChanged) {
        device->setDeviceParam(dp, v);
        if (dp == DeviceParam::SoapySDRDriverArgs)
            openDevice(CDeviceID::SOAPYSDR,1);
    }
}

void CRadioController::play(QString channel, QString title, quint32 service)
{
    if (channel == "") return;

    currentTitle = title;
    emit titleChanged();

    qDebug() << "RadioController:" << "Play:" << title << serialise_serviceid(service) << "on channel" << channel;

    if (isChannelScan == true) stopScan();

    bool isRestartOk = deviceRestart();
    setChannel(channel, false);
    setService(service);

    currentLastChannel = QStringList() << serialise_serviceid(service) << channel;
    QSettings settings;
    settings.setValue("lastchannel", currentLastChannel);

    if (isRestartOk) {
        isPlaying = true;
        emit isPlayingChanged(isPlaying);
    } else {
        resetTechnicalData();
        currentTitle = title;
        emit titleChanged();
        currentText = tr("Playback failed");
        emit textChanged();
    }
}

void CRadioController::stop()
{
    if (radioReceiver) radioReceiver->stop();
    if (device) device->stop();
    else throw std::runtime_error("device is null");

    QString title = currentTitle;
    resetTechnicalData();
    currentTitle = title;
    emit titleChanged();
    currentText = tr("Stopped");
    emit textChanged();

    audio.stop();
    labelTimer.stop();

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

    qDebug() << "RadioController:" << "Start channel scan";

    stop();
    deviceRestart();

    if(device && device->getID() == CDeviceID::RAWFILE) {
        currentTitle = tr("RAW File");
        const auto FirstChannel = QString::fromStdString(Channels::firstChannel);
        setChannel(FirstChannel, false);
        emit scanStopped();
    }
    else {
        QString Channel = QString::fromStdString(Channels::firstChannel);
        setChannel(Channel, true);
        isChannelScan = true;
        emit isChannelScanChanged(isChannelScan);
        stationCount = 0;
        currentTitle = tr("Scanning") + " ... " + Channel + " (" + QString::number((1 * 100 / NUMBEROFCHANNELS)) + "%)";
        emit titleChanged();
        currentText = tr("Found channels") + ": " + QString::number(stationCount);
        emit textChanged();
        currentService = 0;
        emit stationChanged();
        currentStationType = "";
        emit stationTypChanged();
        currentLanguageType = "";
        emit languageTypeChanged();
        emit scanProgress(0);
    }
}

void CRadioController::stopScan(void)
{
    qDebug() << "RadioController:" << "Stop channel scan";
    currentTitle = tr("No Station");
    emit titleChanged();
    currentText = "";
    emit textChanged();
    isChannelScan = false;
    emit isChannelScanChanged(isChannelScan);
    emit scanStopped();
    stop();
}

void CRadioController::setAGC(bool isAGC)
{
    this->isAGC = isAGC;
    if (device) {
        device->setAgc(isAGC);
        if (!isAGC) {
            device->setGain(currentManualGain);
        }
    }
    emit agcChanged(isAGC);
}

// ... (Rest of your implementation functions like setChannel, setService, etc. remain unchanged) ...
// Ensure the final closing brace exists for the CRadioController class.
