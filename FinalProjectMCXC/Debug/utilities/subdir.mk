################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../utilities/fsl_assert.c 

C_DEPS += \
./utilities/fsl_assert.d 

OBJS += \
./utilities/fsl_assert.o 


# Each subdirectory must supply rules for building sources it contributes
utilities/%.o: ../utilities/%.c utilities/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_MCXC444VLH -DCPU_MCXC444VLH_cm0plus -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -DSDK_DEBUGCONSOLE_UART -DSERIAL_PORT_TYPE_UART=1 -DSDK_OS_FREE_RTOS -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\board" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\source" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\drivers" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\CMSIS" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\CMSIS\m-profile" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\utilities" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\utilities\debug_console\config" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\device" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\device\periph2" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\utilities\debug_console" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\component\serial_manager" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\component\lists" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\utilities\str" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\component\uart" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\freertos\freertos-kernel\include" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\freertos\freertos-kernel\portable\GCC\ARM_CM0" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\freertos\freertos-kernel\template" -I"C:\Users\User\code\CG2271-Project\FinalProjectMCXC\freertos\freertos-kernel\template\ARM_CM0" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -ffunction-sections -fdata-sections -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m0plus -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-utilities

clean-utilities:
	-$(RM) ./utilities/fsl_assert.d ./utilities/fsl_assert.o

.PHONY: clean-utilities

