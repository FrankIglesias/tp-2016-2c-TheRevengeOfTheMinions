################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../bitarray.c \
../config.c \
../error.c \
../log.c \
../nivel.c \
../process.c \
../string.c \
../sw_sockets.c \
../tad_items.c \
../temporal.c \
../tiposDato.c \
../txt.c 

OBJS += \
./bitarray.o \
./config.o \
./error.o \
./log.o \
./nivel.o \
./process.o \
./string.o \
./sw_sockets.o \
./tad_items.o \
./temporal.o \
./tiposDato.o \
./txt.o 

C_DEPS += \
./bitarray.d \
./config.d \
./error.d \
./log.d \
./nivel.d \
./process.d \
./string.d \
./sw_sockets.d \
./tad_items.d \
./temporal.d \
./tiposDato.d \
./txt.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


