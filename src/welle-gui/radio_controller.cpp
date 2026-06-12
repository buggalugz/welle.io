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
 * Many of the ideas as implemented in welle.io are derived from
 * other work, made available through the GNU general Public License.
 * All copyrights of the original authors are recognized.
 *
 * welle.io is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * welle.io is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with welle.io; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
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
    // Init the technical data
    resetTechnicalData();

    // Init timers
    connect(&labelTimer, &QTimer::timeout, this, &CRadioController::labelTimerTimeout);
    connect(&stationTimer, &QTimer::timeout, this, &CRadioController::stationTimerTimeout);
    connect(&channelTimer, &QTimer::timeout, this, &CRadioController::channelTimerTimeout);

    // Use the signal slot mechanism is necessary because the backend runs in a different thread
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

    // Reset the technical data
    resetTechnicalData();

    emit deviceClosed();
}

CDeviceID CRadioController::openDevice(CDeviceID deviceId, bool force, QVariant param1, QVariant param2)
{
    if(this->deviceId != deviceId || force) {
        closeDevice();
        device.reset(CInputFactory::GetDevice(*this, deviceId));

        // Set rtl_tcp settings
        if (device->getID() == CDeviceID::RTL_TCP) {
            CRTL_TCP_Client* RTL_TCP_Client = static_cast<CRTL_TCP_Client*>(device.get());

            RTL_TCP_Client->setServerAddress(param1.toString().toStdString());
            RTL_TCP_Client->setPort(param2.toInt());
        }

        // Set rtl_tcp settings
        if (device->getID() == CDeviceID::RAWFILE) {
            CRAWFile* rawFile = static_cast<CRAWFile*>(device.get());
#ifdef __ANDROID__
            // Using QFile is necessary to get access to com.android.externalstorage.ExternalStorageProvider
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
