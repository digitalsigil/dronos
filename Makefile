#
# Defines the part type that this project uses.
#
PART=TM4C123GH6PM

#
# The base directory for TivaWare.
#
ROOT=/home/rc/sw/tiva

#
# Include the common make definitions.
#
include ${ROOT}/makedefs

#
# Where to find source files that do not live in this directory.
#
VPATH=${ROOT}/third_party/FreeRTOS/Source/portable/GCC/ARM_CM4F
VPATH+=${ROOT}/third_party/FreeRTOS/Source/portable/MemMang/
VPATH+=${ROOT}/third_party/FreeRTOS/Source
VPATH+=${ROOT}/utils

#
# Where to find header files that do not live in the source directory.
#
IPATH=.
IPATH+=${ROOT}
IPATH+=${ROOT}/third_party/FreeRTOS/Source/portable/GCC/ARM_CM4F
IPATH+=${ROOT}/third_party/FreeRTOS
IPATH+=${ROOT}/third_party/FreeRTOS/Source/include
IPATH+=${ROOT}/third_party


## DEFAULT TARGET ##
all: ${COMPILER}
all: ${COMPILER}/dronos.axf

## CLEAN TARGET ##
clean:
	@rm -rf ${COMPILER} ${wildcard *~}

## The rule to create the target directory. ##
${COMPILER}:
	@mkdir -p ${COMPILER}


## build rules for dronos target ##
${COMPILER}/dronos.axf: ${COMPILER}/dronos.o
${COMPILER}/dronos.axf: ${COMPILER}/heap_2.o
${COMPILER}/dronos.axf: ${COMPILER}/list.o
${COMPILER}/dronos.axf: ${COMPILER}/port.o
${COMPILER}/dronos.axf: ${COMPILER}/queue.o
${COMPILER}/dronos.axf: ${COMPILER}/startup_${COMPILER}.o
${COMPILER}/dronos.axf: ${COMPILER}/vision_task.o
${COMPILER}/dronos.axf: ${COMPILER}/power_task.o
${COMPILER}/dronos.axf: ${COMPILER}/tasks.o
${COMPILER}/dronos.axf: ${COMPILER}/uartstdio.o
${COMPILER}/dronos.axf: ${COMPILER}/ustdlib.o
${COMPILER}/dronos.axf: ${ROOT}/driverlib/${COMPILER}/libdriver.a
${COMPILER}/dronos.axf: dronos.ld
SCATTERgcc_dronos=dronos.ld
ENTRY_dronos=ResetISR
CFLAGSgcc=-DTARGET_IS_TM4C123_RB1

# Include the automatically generated dependency files.
ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif
