
BUILD_ALL += $(OUT_DIR)/ref_app_wave_to_exit

$(OUT_DIR)/ref_app_wave_to_exit : \
					$(OUT_OBJ_DIR)/ref_app_wave_to_exit.o \
					libacconeer.a \
					libcustomer.a \
					libacc_detector_presence.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@

