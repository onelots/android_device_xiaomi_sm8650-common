#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# GMS
PRODUCT_GMS_CLIENTID_BASE := android-xiaomi

# Shipping API
BOARD_SHIPPING_API_LEVEL := 202402
PRODUCT_SHIPPING_API_LEVEL := 34

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH)

# Inherit from the proprietary files makefile.
$(call inherit-product, vendor/xiaomi/sm8650-common/sm8650-common-vendor.mk)
