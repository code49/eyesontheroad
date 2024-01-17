
BUILD_ALL += $(OUT_DIR)/ref_app_tank_level

$(OUT_DIR)/ref_app_tank_level : \
					$(OUT_OBJ_DIR)/ref_app_tank_level.o \
					libacc_detector_distance.a \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
