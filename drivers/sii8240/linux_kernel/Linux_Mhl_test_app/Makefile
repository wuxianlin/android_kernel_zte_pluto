#
# Makefile for the Silicon Image 8334 MHL TX device driver
# test application
#

OBJS += \
siidrvtest.o

INCLUDES := -I/usr/loca//angstrom/arm/arm-angstrom-linux-gnueabi/usr/include
INCLUDES += -I../linux_kernel/public

.PHONY:	clean

all:		siidrvtest
		@echo 'make all complete'

clean:
		rm -rf siidrvtest *.o *.d
		@echo 'make complete'

%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
#	arm-angstrom-linux-gnueabi-gcc -I/usr/loca//angstrom/arm/arm-angstrom-linux-gnueabi/usr/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	arm-angstrom-linux-gnueabi-gcc $(INCLUDES) -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


siidrvtest: $(OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	arm-angstrom-linux-gnueabi-gcc -lrt -o"siidrvtest" $(OBJS)
	@echo 'Finished building target: $@'
	@echo ' '

