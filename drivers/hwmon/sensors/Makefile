
# Kernel makefile for zte sensors
#
#

obj-$(CONFIG_ACCEL_SENSORS)	+= accel/

obj-$(CONFIG_ZTE_SENSORS)	+= zte_sensors.o
zte_sensors-objs += sensors.o
zte_sensors-objs += sensors-check.o

ifdef CONFIG_SENSORS_TMD2771
zte_sensors-objs += light/tmd2771x.o
endif

ifdef CONFIG_SENSORS_AK8962
zte_sensors-objs += compass/akm8962.o
endif

ifdef CONFIG_SENSORS_L3G4200D
zte_sensors-objs += gyro/l3g4200d.o
endif

ifdef CONFIG_SENSORS_LSM330D_G
zte_sensors-objs += gyro/lsm330dlc_gyr.o
endif
