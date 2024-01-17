
BUILD_ALL += $(OUT_DIR)/example_service_power_bins

$(OUT_DIR)/example_service_power_bins : \
					$(OUT_OBJ_DIR)/example_service_power_bins.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
