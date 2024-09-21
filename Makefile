PROJECT_TARGET = "stm32l4"
PROJECT_BUILD_DIR = $(CURDIR)/build
PROJECT_C_SOURCES = $(CURDIR)/Src
PROJECT_C_INCLUDES = $(CURDIR)/Inc

all: 
	$(MAKE) $(CURDIR)/embedded-sharepoint/

BUILD_DIR = $(PROJECT_BUILD_DIR)

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)


#######################################
# flash
#######################################
FLASH_ADDRESS ?= 0x8000000

flash:
	-st-flash write $(BUILD_DIR)/$(TARGET).bin $(FLASH_ADDRESS)

#######################################
# format
#######################################
FORMAT_CONFIG ?= --style=file:../.clang-format

format:
	-clang-format $(FORMAT_CONFIG) $(CLANG_INPUTS)

format-fix:
	-clang-format -i $(FORMAT_CONFIG) $(CLANG_INPUTS)


#######################################
# help
#######################################
help:
	@echo "Available targets:"
	@echo "  all          - Build the project."
	@echo "  clean        - Remove build artifacts."
	@echo "  flash        - Flash the target device."
	@echo "  tidy         - Run clang-tidy."
	@echo "  tidy-fix     - Run clang-tidy with fixes."
	@echo "  format       - Run clang-format."
	@echo "  format-fix   - Run clang-format and apply fixes."


#######################################