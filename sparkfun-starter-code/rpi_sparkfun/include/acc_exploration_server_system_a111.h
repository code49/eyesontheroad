// Copyright (c) Acconeer AB, 2021
// All rights reserved

#ifndef ACC_EXPLORATION_SERVER_SYSTEM_A111_H_
#define ACC_EXPLORATION_SERVER_SYSTEM_A111_H_

/*
 * Below functions should be called prior to calling acc_exploration_server_init()
 * to register the desired services. Services that are not registered will not be supported
 * and the linker can discard the code which reduces the binary size.
 */
void acc_exploration_server_register_all_services(void);


void acc_exploration_server_register_sparse_service(void);


void acc_exploration_server_register_envelope_service(void);


void acc_exploration_server_register_power_bins_service(void);


void acc_exploration_server_register_iq_service(void);


#endif
