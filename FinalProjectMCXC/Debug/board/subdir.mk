################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../board/board.c \
../board/clock_config.c \
../board/peripherals.c \
../board/pin_mux.c 

C_DEPS += \
./board/board.d \
./board/clock_config.d \
./board/peripherals.d \
./board/pin_mux.d 

OBJS += \
./board/board.o \
./board/clock_config.o \
./board/peripherals.o \
./board/pin_mux.o 


# Each subdirectory must supply rules for building sources it contributes
board/%.o: ../board/%.c board/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_MCXC444VLH -DCPU_MCXC444VLH_cm0plus -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -DSDK_DEBUGCONSOLE_UART -DSERIAL_PORT_TYPE_UART=1 -DSDK_OS_FREE_RTOS -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\board" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\source" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\drivers" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\CMSIS" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\CMSIS\m-profile" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities\debug_console\config" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\device" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\device\periph2" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities\debug_console" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\component\serial_manager" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\component\lists" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\utilities\str" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\component\uart" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\include" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\portable\GCC\ARM_CM0" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\template" -I"C:\Users\vetri\Documents\MCUXpressoIDE_25.6.136\workspace\FinalProject\freertos\freertos-kernel\template\ARM_CM0" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -ffunction-sections -fdata-sections -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m0plus -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-board

clean-board:
	-$(RM) ./board/board.d ./board/board.o ./board/clock_config.d ./board/clock_config.o ./board/peripherals.d ./board/peripherals.o ./board/pin_mux.d ./board/pin_mux.o

.PHONY: clean-board

