
BUILD_ALL += utils/acc_service_data_logger

utils/acc_service_data_logger : \
					$(OUT_OBJ_DIR)/acc_service_data_logger.o \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)mkdir -p utils
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
