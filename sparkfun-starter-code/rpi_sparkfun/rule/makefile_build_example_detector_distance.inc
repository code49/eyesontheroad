
BUILD_ALL += $(OUT_DIR)/example_detector_distance

$(OUT_DIR)/example_detector_distance : \
					$(OUT_OBJ_DIR)/example_detector_distance.o \
					libacc_detector_distance.a \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
