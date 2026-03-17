# Ensure OS detection is linked (config.h sets OS_DETECTION_ENABLE)
SRC += $(QUANTUM_DIR)/os_detection.c

LTO_ENABLE = yes
DEBOUNCE_TYPE = sym_defer_pk