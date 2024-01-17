
BUILD_ALL += $(OUT_DIR)/ref_app_rf_certification_test

$(OUT_DIR)/ref_app_rf_certification_test : \
					$(OUT_OBJ_DIR)/ref_app_rf_certification_test.o \
					libacc_rf_certification_test_a111.a \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
