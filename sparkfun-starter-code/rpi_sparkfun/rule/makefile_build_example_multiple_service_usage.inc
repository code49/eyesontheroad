
BUILD_ALL += $(OUT_DIR)/example_multiple_service_usage

$(OUT_DIR)/example_multiple_service_usage : \
					$(OUT_OBJ_DIR)/example_multiple_service_usage.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
