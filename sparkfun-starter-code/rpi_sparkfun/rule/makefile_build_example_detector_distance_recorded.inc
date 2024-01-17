
BUILD_ALL += $(OUT_DIR)/example_detector_distance_recorded

$(OUT_DIR)/example_detector_distance_recorded : \
					$(OUT_OBJ_DIR)/example_detector_distance_recorded.o \
					libacc_detector_distance.a \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
