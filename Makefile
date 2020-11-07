BINARY_NAME:= sample-trigger

all: $(BINARY_NAME)

# $@ target
# $< first dependency
# $^ all dependencies
# $? all dependencies more recent than the target

CFLAGS  += -O0 -Dposix -ggdb -O2 -msoft-float -Wall -Wlogical-op -Wtype-limits -Wsign-compare -Wshadow -Wpointer-arith -Wstrict-prototypes -I . -I $(SDKTARGETSYSROOT)/usr/include
LDFLAGS += -lsndfile -lasound -lpthread -lrt -ldl -lm
LDFLAGS += -L $(SDKTARGETSYSROOT)/usr/lib -L $(SDKTARGETSYSROOT)/lib

$(BINARY_NAME): $(BINARY_NAME).o hal_alsa.o hal_sndfile.o log.o hal_mqueue.o
	$(CC) $^ $(LDFLAGS) -o $@

$(BINARY_NAME).o: $(BINARY_NAME).c
	$(CC) $(CFLAGS) -c $<

clean:
	@rm -f $(BINARY_NAME)
	@find . -name \*~ -print | xargs rm -rf
	@find . -name \*.o -print | xargs rm -rf

adb_shell_enable:
	adb wait-for-device
	adb push ../adb_credentials /var/run/

install_target: $(BINARY_NAME) adb_shell_enable
	@echo Copying to target
	adb push $(BINARY_NAME) /cache

.PHONY: clean
