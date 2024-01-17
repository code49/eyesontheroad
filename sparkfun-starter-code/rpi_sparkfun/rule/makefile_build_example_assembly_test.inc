
BUILD_ALL += $(OUT_DIR)/example_assembly_test

$(OUT_DIR)/example_assembly_test : \
					$(OUT_OBJ_DIR)/example_assembly_test.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
