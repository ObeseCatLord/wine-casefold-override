CC ?= cc
STRIP ?= strip
CFLAGS ?= -O2 -g -fPIC -Wall -Wextra -Werror -std=c11
LDFLAGS ?= -shared -Wl,-z,defs -Wl,-z,relro -Wl,-z,now
BUILD_DIR := build
DIST_DIR := dist
LIB := $(BUILD_DIR)/libwinecasefold.so
LIB32 := $(BUILD_DIR)/libwinecasefold32.so
TEST := $(BUILD_DIR)/test_casefold_override
TEST32 := $(BUILD_DIR)/test_casefold_override32
RELEASE64 := $(DIST_DIR)/libwinecasefold-linux-x86_64.so
RELEASE32 := $(DIST_DIR)/libwinecasefold-linux-i686.so

.PHONY: all build32 test test32 release clean

all: $(LIB) $(TEST)

$(BUILD_DIR):
	mkdir -p $@

$(LIB): src/wine_casefold_override.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(LIB32): src/wine_casefold_override.c | $(BUILD_DIR)
	$(CC) -m32 $(CFLAGS) $(LDFLAGS) -o $@ $<

$(TEST32): tests/test_casefold_override.c | $(BUILD_DIR)
	$(CC) -m32 -O2 -g -Wall -Wextra -Werror -std=c11 -o $@ $<

build32: $(LIB32) $(TEST32)

$(TEST): tests/test_casefold_override.c | $(BUILD_DIR)
	$(CC) -O2 -g -Wall -Wextra -Werror -std=c11 -o $@ $<

test: all
	tests/run-tests.sh

test32: build32
	TEST_LIBRARY="$(LIB32)" TEST_BINARY="$(TEST32)" tests/run-tests.sh

release: test test32
	mkdir -p $(DIST_DIR)
	install -m 0755 $(LIB) $(RELEASE64)
	install -m 0755 $(LIB32) $(RELEASE32)
	$(STRIP) --strip-unneeded $(RELEASE64) $(RELEASE32)
	cd $(DIST_DIR) && sha256sum $(notdir $(RELEASE64)) $(notdir $(RELEASE32)) > SHA256SUMS

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
