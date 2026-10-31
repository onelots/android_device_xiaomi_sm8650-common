/*
 * Copyright (C) 2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "UdfpsHandler.xiaomi_sm8650"

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <aidl/vendor/xiaomi/hw/touchfeature/ITouchFeature.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <android/binder_manager.h>

#include <chrono>
#include <fstream>
#include <thread>

#include "UdfpsHandler.h"

#define COMMAND_NIT 10
#define PARAM_NIT_FOD 1
#define PARAM_NIT_NONE 0

#define COMMAND_FOD_PRESS_STATUS 1
#define COMMAND_FOD_PRESS_X 2
#define COMMAND_FOD_PRESS_Y 3
#define PARAM_FOD_PRESSED 1
#define PARAM_FOD_RELEASED 0

#define FOD_STATUS_PATH "/sys/class/touch/touch_dev/fod_press_status"
#define FOD_STATUS_OFF 0
#define FOD_STATUS_ON 1

#define DISP_PARAM_PATH "/sys/devices/virtual/mi_display/disp_feature/disp-DSI-0/disp_param"
#define DISP_PARAM_LOCAL_HBM_MODE "9"
#define DISP_PARAM_LOCAL_HBM_OFF "0"
#define DISP_PARAM_LOCAL_HBM_ON "1"

#define TOUCH_ID 0
#define TOUCH_MODE_FOD_ENABLE 10

#define FINGERPRINT_ACQUIRED_VENDOR 7

using ::aidl::android::hardware::biometrics::fingerprint::AcquiredInfo;
using ::aidl::vendor::xiaomi::hw::touchfeature::ITouchFeature;

namespace {

template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value;
}

}  // anonymous namespace

class XiaomiSM8650UdfpsHander : public UdfpsHandler {
  public:
    void init(fingerprint_device_t* device) {
        mDevice = device;
    }

    void onFingerDown(uint32_t x, uint32_t y, float /*minor*/, float /*major*/) {
        if (mAuthSuccess) return;
        LOG(INFO) << __func__ << " x: " << x << " y: " << y;
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_X, x);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_Y, y);
        setFingerDown(true);
    }

    void onFingerUp() {
        LOG(INFO) << __func__;
        setFingerDown(false);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_X, 0);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_Y, 0);
    }

    void onAuthenticationSucceeded() {
        LOG(INFO) << __func__;
        mAuthSuccess = true;
        onFingerUp();
        setFodStatus(FOD_STATUS_OFF);
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            mAuthSuccess = false;
        }).detach();
    }

    void onAuthenticationFailed() {
        LOG(INFO) << __func__;
        onFingerUp();
    }

    void onAcquired(int32_t result, int32_t vendorCode) {
        LOG(INFO) << __func__ << " result: " << result << " vendorCode: " << vendorCode;
        if (result != FINGERPRINT_ACQUIRED_VENDOR) {
            setFingerDown(false);
            if (static_cast<AcquiredInfo>(result) == AcquiredInfo::GOOD) {
                setFodStatus(FOD_STATUS_OFF);
            }
        } else if (vendorCode == 21 || vendorCode == 23) {
            /*
             * vendorCode = 21 waiting for fingerprint authentication
             * vendorCode = 23 waiting for fingerprint enroll
             */
            mAuthSuccess = false;
            setFodStatus(FOD_STATUS_ON);
        } else if (vendorCode == 44) {
            /*
             * vendorCode = 44 fingerprint scan failed
             */
            setFingerDown(false);
        }
    }

    void cancel() {
        LOG(INFO) << __func__;
        mAuthSuccess = false;
        setFingerDown(false);
        setFodStatus(FOD_STATUS_OFF);
    }

  private:
    fingerprint_device_t* mDevice;
    std::shared_ptr<ITouchFeature> mTouchFeature;
    bool mAuthSuccess = false;

    void setFodStatus(int value) {
        set(FOD_STATUS_PATH, value);
        setTouchFodEnable(value == FOD_STATUS_ON);
    }

    void setTouchFodEnable(bool enable) {
        if (!mTouchFeature) {
            const std::string instance = std::string(ITouchFeature::descriptor) + "/default";
            mTouchFeature = ITouchFeature::fromBinder(
                    ndk::SpAIBinder(AServiceManager_checkService(instance.c_str())));
            if (!mTouchFeature) {
                LOG(ERROR) << "Failed to get " << instance;
                return;
            }
        }

        /*
         * Keep FOD gesture enabled in the touch IC while authenticating,
         * otherwise it enters deep sleep once the panel dozes.
         */
        int32_t ret;
        mTouchFeature->setModeValue(TOUCH_ID, TOUCH_MODE_FOD_ENABLE, enable ? 1 : 0, &ret);
    }

    void setFingerDown(bool pressed) {
        mDevice->extCmd(mDevice, COMMAND_NIT, pressed ? PARAM_NIT_FOD : PARAM_NIT_NONE);

        set(DISP_PARAM_PATH,
            std::string(DISP_PARAM_LOCAL_HBM_MODE) + " " +
                    (pressed ? DISP_PARAM_LOCAL_HBM_ON : DISP_PARAM_LOCAL_HBM_OFF));

        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_STATUS,
                        pressed ? PARAM_FOD_PRESSED : PARAM_FOD_RELEASED);
    }
};

static UdfpsHandler* create() {
    return new XiaomiSM8650UdfpsHander();
}

static void destroy(UdfpsHandler* handler) {
    delete handler;
}

extern "C" UdfpsHandlerFactory UDFPS_HANDLER_FACTORY = {
        .create = create,
        .destroy = destroy,
};
