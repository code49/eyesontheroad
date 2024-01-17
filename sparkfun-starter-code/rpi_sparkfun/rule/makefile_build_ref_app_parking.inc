
BUILD_ALL += $(OUT_DIR)/ref_app_parking

$(OUT_DIR)/ref_app_parking : \
					$(OUT_OBJ_DIR)/ref_app_parking.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
