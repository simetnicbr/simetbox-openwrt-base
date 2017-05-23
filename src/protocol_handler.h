/** ----------------------------------------
 * @file  protocol_handler.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 23, 2011
 *------------------------------------------*/


#ifndef PROTOCOL_HANDLER_H_
#define PROTOCOL_HANDLER_H_

/* ###########################################  dispatcher  ######### */
int handle_control_message(Context_t * context, char *message);
int handle_fatal_error(Context_t * context);
void finalize_test_and_start_next(Context_t * context);

#endif /* PROTOCOL_HANDLER_H_ */
