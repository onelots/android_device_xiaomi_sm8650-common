/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/frameworks/sensorservice/1.0/ISensorManager.h>
#include <android/frameworks/sensorservice/1.0/types.h>
#include <thread>

using ::android::frameworks::sensorservice::V1_0::IEventQueue;
using ::android::frameworks::sensorservice::V1_0::IEventQueueCallback;
using ::android::frameworks::sensorservice::V1_0::ISensorManager;
using ::android::frameworks::sensorservice::V1_0::Result;
using android::sp;

class SensorNotifier {
  public:
    SensorNotifier(sp<ISensorManager> manager);
    virtual ~SensorNotifier();
    void activate();
    void deactivate();

  protected:
    virtual void notify() = 0;
    void initializeSensorQueue(const std::string& type, bool wakeup,
                               IEventQueueCallback* callback);

    sp<ISensorManager> mManager;
    sp<IEventQueue> mQueue;
    int32_t mSensorHandle = -1;
    bool mActive = false;
    std::thread mThread;
};
