
include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Module project related part

ifeq ($(CONFIG_MTK_SENSOR_SUPPORT),y)
ifeq ($(CONFIG_MTK_APDS9930),y)
obj-y	+=  alsps/APDS9930/
endif

#ifeq ($(CONFIG_MTK_BMA250),y)
obj-y   +=  accelerometer/bma250/
#endif

ifeq ($(CONFIG_MTK_MPU3050C),y)
obj-y   +=  gyroscope/mpu3050c/
endif

ifeq ($(CONFIG_MTK_AKM8963),y)
obj-y	+=  magnetometer/akm8963/
endif
endif
ifneq ($(CONFIG_ARCH_MTK_PROJECT),)
ARCH_MTK_PROJECT := $(shell echo $(CONFIG_ARCH_MTK_PROJECT))
obj-y	+= core/
obj-y	+= dct/dct/
endif

ifeq ($(CONFIG_MTK_HDMI_SUPPORT),y)
obj-y	+=  hdmi/
endif

obj-y	+=  camera/camera/
obj-y   +=  flashlight/constant_flashlight/
obj-y	+=  vibrator/
obj-y	+=  leds/
obj-y	+=  keypad/
obj-y	+=  sound/
obj-y	+=  accdet/
obj-y   +=  partition/

ifeq ($(CONFIG_MTK_MULTIBRIDGE_SUPPORT),y)
obj-y	+=  multibridge/mt8193/
endif
