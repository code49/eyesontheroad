BUILD_LIBS += $(OUT_LIB_DIR)/libcustomer.a

$(OUT_LIB_DIR)/libcustomer.a: \
			$(addprefix $(OUT_OBJ_DIR)/,$(notdir $(patsubst %.c,%.o,$(sort $(wildcard source/acc_heap_*.c))))) \
			$(addprefix $(OUT_OBJ_DIR)/,$(notdir $(patsubst %.c,%.o,$(sort $(wildcard source/acc_integration_*.c))))) \
			$(addprefix $(OUT_OBJ_DIR)/,$(notdir $(patsubst %.c,%.o,$(sort $(wildcard source/acc_board_*.c))))) \
			$(addprefix $(OUT_OBJ_DIR)/,$(notdir $(patsubst %.c,%.o,$(sort $(wildcard source/acc_lib*.c)))))
	@echo "    Creating archive $(notdir $@)"
	$(SUPPRESS)rm -f $@
	$(SUPPRESS)$(TOOLS_AR) $(ARFLAGS) $@ $^
