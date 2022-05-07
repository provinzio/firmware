#ifndef __MACRO_SET_COMMAND_H__
#define __MACRO_SET_COMMAND_H__

// Includes:

    #include <stdint.h>
    #include <stdbool.h>
    #include "key_action.h"
    #include "usb_device_config.h"
    #include "key_states.h"
    #include "macros.h"

// Macros:

// Typedefs:

   typedef enum {
       SetMode_Set,
       SetMode_Toggle,
       SetMode_Adjust,
   } set_mode_t;

// Variables:

// Functions:
    macro_result_t MacroSetCommand(const char* text, const char *textEnd, set_mode_t mode);

#endif /* __MACRO_SET_COMMAND_H__ */
