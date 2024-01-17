
BUILD_ALL += $(OUT_DIR)/example_error_handling

$(OUT_DIR)/example_error_handling : \
					$(OUT_OBJ_DIR)/example_error_handling.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
