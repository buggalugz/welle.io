#ifndef RADIO_CONTROLLER_H
#define RADIO_CONTROLLER_H

#include <QObject>
#include <QVariant>
#include <QVariantMap>
#include <QTimer>
#include <QFile>
#include <QScopedPointer>
#include <memory>
#include "radio_receiver.h" // Assuming this exists in your project
#include "input_device.h"   // Assuming this exists

class CRadioController : public QObject
{
    Q_OBJECT
public:
    explicit CRadioController(QVariantMap& commandLineOptions, QObject *parent = nullptr);
    ~CRadioController();

    void stop();
    void startScan();
    void stopScan();
    void setAGC(bool isAGC);
    void closeDevice();
    CDeviceID openDevice(CDeviceID deviceId, bool force = false, QVariant param1 = QVariant(), QVariant param2 = QVariant());
    CDeviceID openDevice();
    void setDeviceParam(QString param, int value);
    void setDeviceParam(QString param, QString value);
    void play(QString channel, QString title, quint32 service);

signals:
    void deviceClosed();
    void isPlayingChanged(bool isPlaying);
    void isChannelScanChanged(bool isChannelScan);
    void scanStopped();
    void titleChanged();
    void textChanged();
    void scanProgress(int progress);
    void stationChanged();
    void stationTypChanged();
    void languageTypeChanged();
    void agcChanged(bool isAGC);
    void switchToNextChannel();
    void ensembleIdUpdated(quint32 id);
    void ensembleLabelUpdated(DabLabel label);
    void serviceDetected(quint32 id);
    void dateTimeUpdated(dab_date_time_t dt);
    void restartServiceRequested();

private:
    QVariantMap commandLineOptions;
    QScopedPointer<CInputDevice> device;
    QScopedPointer<CRadioReceiver> radioReceiver;
    QScopedPointer<QFile> rawFileAndroid;
    bool isChannelScan = false;
    bool isPlaying = false;
    bool isAGC = true;
    int currentManualGain = 0;
    
    // Internal timer/helper methods
    void resetTechnicalData();
    bool deviceRestart();
    void initialise();
    void setChannel(QString channel, bool scan);
    void setService(quint32 service);
    
    QTimer labelTimer, stationTimer, channelTimer;
    
    // Add your existing private members here if they were specific
};

#endif // RADIO_CONTROLLER_H
