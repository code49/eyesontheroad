
BUILD_ALL += $(OUT_DIR)/example_service_sparse

$(OUT_DIR)/example_service_sparse : \
					$(OUT_OBJ_DIR)/example_service_sparse.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
