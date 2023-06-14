# Source files
SRC_DIR := source
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)

# Header files
HEADER_DIR := header
HEADER_FILES := $(wildcard $(HEADER_DIR)/*.h)

# Test files
TEST_DIR := test
TEST_SRC_FILES := $(wildcard $(TEST_DIR)/*.c)

# Compilator and flags
CC := $(or $(CC),gcc)
CFLAGS := -Wall -Wextra

# Libraries
LIBS := -pthread -lm

# Flags for clang
ifeq ($(CC),clang)
	CFLAGS := -Weverything -Wno-declaration-after-statement
endif

# Binary file
TARGET := cpuUsageTracker
TEST_TARGET := cpuUsageTrackerTest

# Object files
OBJ_FILES := $(SRC_FILES:.c=.o)
TEST_OBJ_FILES := $(TEST_SRC_FILES:.c=.o)

# Temporary files
TMP_FILES := $(SRC_FILES:.c=*.tmp) $(TEST_SRC_FILES:.c=*.tmp)

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@$(MAKE) clean_objects

$(TEST_TARGET): $(TEST_OBJ_FILES) $(filter-out $(SRC_DIR)/cpuUsageTracker.o, $(OBJ_FILES))
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@$(MAKE) clean_objects

%.o: %.c
	$(CC) $(CFLAGS) -I$(HEADER_DIR) -c -o $@ $<

clean:
	rm -f $(TARGET) $(TEST_TARGET)

clean_objects:
	rm -f $(OBJ_FILES) $(TEST_OBJ_FILES) $(TMP_FILES)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

.PHONY: all clean clean_objects test
