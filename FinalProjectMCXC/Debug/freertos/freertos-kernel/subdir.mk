################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../freertos/freertos-kernel/croutine.c \
../freertos/freertos-kernel/event_groups.c \
../freertos/freertos-kernel/list.c \
../freertos/freertos-kernel/queue.c \
../freertos/freertos-kernel/stream_buffer.c \
../freertos/freertos-kernel/tasks.c \
../freertos/freertos-kernel/timers.c 

C_DEPS += \
./freertos/freertos-kernel/croutine.d \
./freertos/freertos-kernel/event_groups.d \
./freertos/freertos-kernel/list.d \
./freertos/freertos-kernel/queue.d \
./freertos/freertos-kernel/stream_buffer.d \
./freertos/freertos-kernel/tasks.d \
./freertos/freertos-kernel/timers.d 

OBJS += \
./freertos/freertos-kernel/croutine.o \
./freertos/freertos-kernel/event_groups.o \
./freertos/freertos-kernel/list.o \
./freertos/freertos-kernel/queue.o \
./freertos/freertos-kernel/stream_buffer.o \
./freertos/freertos-kernel/tasks.o \
./freertos/freertos-kernel/timers.o 


# Each subdirectory must supply rules for building sources it contributes
freertos/freertos-kernel/%.o: ../freertos/freertos-kernel/%.c freertos/freertos-kernel/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_MCXC444VLH -DCPU_MCXC444VLH_cm0plus -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -DSDK_DEBUGCONSOLE_UART -DSERIAL_PORT_TYPE_UART=1 -DSDK_OS_FREE_RTOS -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\board" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\source" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\drivers" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\CMSIS" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\CMSIS\m-profile" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities\debug_console\config" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\device" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\device\periph2" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities\debug_console" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\component\serial_manager" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\component\lists" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities\str" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\component\uart" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\include" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\portable\GCC\ARM_CM0" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\template" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\template\ARM_CM0" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -ffunction-sections -fdata-sections -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m0plus -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-freertos-2f-freertos-2d-kernel

clean-freertos-2f-freertos-2d-kernel:
	-$(RM) ./freertos/freertos-kernel/croutine.d ./freertos/freertos-kernel/croutine.o ./freertos/freertos-kernel/event_groups.d ./freertos/freertos-kernel/event_groups.o ./freertos/freertos-kernel/list.d ./freertos/freertos-kernel/list.o ./freertos/freertos-kernel/queue.d ./freertos/freertos-kernel/queue.o ./freertos/freertos-kernel/stream_buffer.d ./freertos/freertos-kernel/stream_buffer.o ./freertos/freertos-kernel/tasks.d ./freertos/freertos-kernel/tasks.o ./freertos/freertos-kernel/timers.d ./freertos/freertos-kernel/timers.o

.PHONY: clean-freertos-2f-freertos-2d-kernel

