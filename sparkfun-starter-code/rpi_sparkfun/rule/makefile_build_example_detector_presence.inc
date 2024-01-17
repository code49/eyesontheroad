
BUILD_ALL += $(OUT_DIR)/example_detector_presence

$(OUT_DIR)/example_detector_presence : \
					$(OUT_OBJ_DIR)/example_detector_presence.o \
					libacc_detector_presence.a \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
