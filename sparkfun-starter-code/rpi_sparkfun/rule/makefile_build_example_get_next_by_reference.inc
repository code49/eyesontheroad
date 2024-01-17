
BUILD_ALL += $(OUT_DIR)/example_get_next_by_reference

$(OUT_DIR)/example_get_next_by_reference : \
					$(OUT_OBJ_DIR)/example_get_next_by_reference.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
