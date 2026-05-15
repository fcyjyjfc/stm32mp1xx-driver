SHELL=C:\Windows\System32\cmd.exe
NAME = Driver_Test
BUILD_DIR = build
OUTPUT_DIR = out

CFLAGS += -g -Wall -O0 -mabi=apcs-gnu -mfpu=neon -mfloat-abi=softfp -fno-builtin \
			-nostdinc -I./include

CROSS_COMPILE = arm-none-eabi-
LD	= $(CROSS_COMPILE)ld
CC	= $(CROSS_COMPILE)gcc
NM  = $(CROSS_COMPILE)nm
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# 收集所有 .c 源文件
C_SRCS :=$(wildcard source/*.c)

# 收集所有 .s 和 .S 汇编源文件（用于 build 目录下的其他汇编文件，如果有）
S_SRCS := $(wildcard asm/*.s)

# 生成 .o 文件路径（只包含 C 文件的目标文件，全部放 build 目录下）
C_OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(C_SRCS:.c=.o)))
OBJS := $(C_OBJS)

# 创建 build 目录
$(BUILD_DIR):
	@mkdir $(BUILD_DIR) 2>nul
	
# 创建 out 目录
$(OUTPUT_DIR):
	@mkdir $(OUTPUT_DIR) 2>nul

# 编译 .c 文件
$(BUILD_DIR)/%.o: source/%.c | $(BUILD_DIR)
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

# 直接编译 asm/start.S 到根目录的 start.o（链接器只在根目录找 start.o）
start.o: asm/start.S
	@echo "  AS      $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

# 默认目标：依赖根目录的 start.o
all: $(BUILD_DIR) $(OUTPUT_DIR) $(OBJS) start.o
	@echo "  OBJS = $(OBJS)"
	@echo "  LD      Linking $(OUTPUT_DIR)/$(NAME).elf"
	@$(LD) $(OBJS) -T $(NAME).lds -o $(OUTPUT_DIR)/$(NAME).elf
	@$(OBJCOPY) -O binary $(OUTPUT_DIR)/$(NAME).elf $(OUTPUT_DIR)/$(NAME).bin
	@$(NM) $(OUTPUT_DIR)/$(NAME).elf > $(OUTPUT_DIR)/$(NAME).map
	@$(OBJDUMP) -DS $(OUTPUT_DIR)/$(NAME).elf > $(OUTPUT_DIR)/$(NAME).dis

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(OUTPUT_DIR)
	@rm -f start.o          # 删除根目录的 start.o
	@echo "  CLEAN   complete."