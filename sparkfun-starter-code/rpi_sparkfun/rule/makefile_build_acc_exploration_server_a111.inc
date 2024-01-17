
BUILD_ALL += out/acc_exploration_server_a111

out/acc_exploration_server_a111 : \
					$(OUT_OBJ_DIR)/acc_exploration_server_linux.o \
					$(OUT_OBJ_DIR)/acc_socket_server.o \
					libacconeer_exploration_server_a111.a \
					libacconeer.a \
					libcustomer.a \

	@echo "    Linking $(notdir $@)"
	$(SUPPRESS)mkdir -p out
	$(SUPPRESS)$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LDLIBS) -o $@
