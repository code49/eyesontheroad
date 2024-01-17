
BUILD_ALL += $(OUT_DIR)/example_detector_obstacle

$(OUT_DIR)/example_detector_obstacle : \
					$(OUT_OBJ_DIR)/example_detector_obstacle.o \
					libacc_detector_obstacle.a \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
