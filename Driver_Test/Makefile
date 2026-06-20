SHELL=C:\Windows\System32\cmd.exe
NAME = Driver_Test
BUILD_DIR = build
OUTPUT_DIR = out

CFLAGS += -g -Wall -O0 -mabi=aapcs -mfpu=neon -mfloat-abi=softfp -fno-builtin \
				-nostdinc -std=c99 -I../drivers/include -I../devices/include
CFLAGS += -I"C:/Program Files (x86)/yagarto/arm-none-eabi/include"
CFLAGS += -I"C:/Program Files (x86)/yagarto/lib/gcc/arm-none-eabi/4.6.2/include"

CROSS_COMPILE = arm-none-eabi-
LD	= $(CROSS_COMPILE)ld
CC	= $(CROSS_COMPILE)gcc
NM  = $(CROSS_COMPILE)nm
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# 收集所有 .c 源文件（test 自己的 + drivers 的 + devices 的）
C_SRCS := $(wildcard source/*.c) $(wildcard ../drivers/source/*.c) $(wildcard ../devices/source/*.c)

# 生成 .o 文件路径（去路径，全部放 build 目录下）
C_OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(C_SRCS:.c=.o)))
OBJS := $(C_OBJS)

$(BUILD_DIR):
	@mkdir $(BUILD_DIR) 2>nul

$(OUTPUT_DIR):
	@mkdir $(OUTPUT_DIR) 2>nul

# 编译 test 自己的 .c 文件（source/ 下）
$(BUILD_DIR)/%.o: source/%.c | $(BUILD_DIR)
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

# 编译 drivers 的 .c 文件
$(BUILD_DIR)/%.o: ../drivers/source/%.c | $(BUILD_DIR)
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

# 编译 devices 的 .c 文件
$(BUILD_DIR)/%.o: ../devices/source/%.c | $(BUILD_DIR)
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

# 编译 asm/start.S 到根目录的 start.o
start.o: asm/start.S
	@echo "  AS      $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

all: $(BUILD_DIR) $(OUTPUT_DIR) $(OBJS) start.o
	@echo "  OBJS = $(OBJS)"
	@echo "  LD      Linking $(OUTPUT_DIR)/$(NAME).elf"
	@$(CC) $(OBJS) -T $(NAME).lds -o $(OUTPUT_DIR)/$(NAME).elf -lc -lgcc -nostartfiles \
		-Wl,-Map=$(OUTPUT_DIR)/$(NAME).map,--cref
	@$(OBJCOPY) -O binary $(OUTPUT_DIR)/$(NAME).elf $(OUTPUT_DIR)/$(NAME).bin
	@$(NM) -n $(OUTPUT_DIR)/$(NAME).elf > $(OUTPUT_DIR)/$(NAME).sym
	@$(OBJDUMP) -DS $(OUTPUT_DIR)/$(NAME).elf > $(OUTPUT_DIR)/$(NAME).dis

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(OUTPUT_DIR)
	@rm -f start.o
	@echo "  CLEAN   complete."
