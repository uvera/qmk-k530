# Ensure OS detection is linked (config.h sets OS_DETECTION_ENABLE)
SRC += $(QUANTUM_DIR)/os_detection.c

# PixArt radio host. custom keeps bluetooth.c's weak hooks and lets k530_bt.c
# supply the real ones. iton_bt speaks a different SPI framing.
BLUETOOTH_ENABLE = yes
BLUETOOTH_DRIVER = custom
SRC += k530_bt.c

LTO_ENABLE = yes
DEBOUNCE_TYPE = sym_defer_pk