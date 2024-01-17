
BUILD_ALL += $(OUT_DIR)/example_service_envelope

$(OUT_DIR)/example_service_envelope : \
					$(OUT_OBJ_DIR)/example_service_envelope.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
