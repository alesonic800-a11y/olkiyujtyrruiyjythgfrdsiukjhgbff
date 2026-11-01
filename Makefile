TARGET      := sushi3ds
APP_TITLE   := Sushi the Cat
APP_DESCRIPTION := Native 3DS reconstruction
APP_AUTHOR  := Community reconstruction
BUILD       := build
SOURCES     := src
INCLUDES    := -Iassets

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH) $(INCLUDES) -D__3DS__
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=3dsx.specs -g -Wl,-Map,$(TARGET).map
LIBS        := -lctru -lm

.PHONY: all 3dsx clean
all: 3dsx
3dsx: $(TARGET).3dsx
include $(DEVKITPRO)/devkitARM/3ds_rules
clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh $(TARGET).map
