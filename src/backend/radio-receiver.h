#ifndef RADIO_RECEIVER_H
#define RADIO_RECEIVER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <vector>

// Define structures used by the receiver
struct DeviceParam {
    enum Type {
        BiasTee,
        SoapySDRAntenna,
        SoapySDRDriverArgs,
        SoapySDRClockSource
    };
};

class CRadioReceiver : public QObject
{
    Q_OBJECT
public:
    explicit CRadioReceiver(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~CRadioReceiver() {}

    // Core functionality
    virtual void start() = 0;
    virtual void stop() = 0;
    
    // Configuration
    virtual void setAgc(bool isAgc) = 0;
    virtual void setGain(int gain) = 0;
    virtual void setDeviceParam(DeviceParam::Type param, int value) = 0;
    virtual void setDeviceParam(DeviceParam::Type param, const std::string& value) = 0;
    
    // Status
    virtual bool isRunning() const = 0;
signals:
    void dataReady(const std::vector<float>& data);
    void errorOccurred(const QString& message);
    void statusChanged(bool running);

protected:
    // Any protected members for derived classes
    bool m_isRunning = false;
};

#endif // RADIO_RECEIVER_H
