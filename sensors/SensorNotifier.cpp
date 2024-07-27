/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SensorNotifier"

#include "SensorNotifier.h"

#include <android-base/logging.h>

SensorNotifier::SensorNotifier(sp<ISensorManager> manager) : mManager(manager) {}

SensorNotifier::~SensorNotifier() {
    deactivate();
}

void SensorNotifier::initializeSensorQueue(const std::string& type, bool wakeup,
                                           IEventQueueCallback* callback) {
    if (mManager == nullptr) {
        LOG(ERROR) << "ISensorManager is null";
        return;
    }

    Return<void> ret = mManager->getSensorList([this, type, wakeup](const auto& list, auto r) {
        if (r != Result::OK) {
            LOG(ERROR) << "failed to get sensor list";
            return;
        }

        for (const auto& sensor : list) {
            if (sensor.stringType == type &&
                (sensor.flags & (1 << 0) /* SENSOR_FLAG_WAKE_UP */) == wakeup) {
                mSensorHandle = sensor.sensorHandle;
                break;
            }
        }
    });

    if (!ret.isOk()) {
        LOG(ERROR) << "failed to get sensor list: " << ret.description();
        return;
    }

    if (mSensorHandle == -1) {
        LOG(ERROR) << "sensor " << type << " not found";
        return;
    }

    Result res;
    ret = mManager->createEventQueue(callback, [this, &res](const auto& q, auto r) {
        mQueue = q;
        res = r;
    });

    if (!ret.isOk() || res != Result::OK) {
        LOG(ERROR) << "failed to create event queue";
        return;
    }
}

void SensorNotifier::activate() {
    if (mActive) {
        return;
    }

    mActive = true;
    mThread = std::thread(&SensorNotifier::notify, this);
}

void SensorNotifier::deactivate() {
    if (!mActive) {
        return;
    }

    mActive = false;
    if (mThread.joinable()) {
        mThread.join();
    }
}
