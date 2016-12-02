################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../pkmn/battle.c \
../pkmn/factory.c 

OBJS += \
./pkmn/battle.o \
./pkmn/factory.o 

C_DEPS += \
./pkmn/battle.d \
./pkmn/factory.d 


# Each subdirectory must supply rules for building sources it contributes
pkmn/%.o: ../pkmn/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


