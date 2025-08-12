/* GNUstep Windows compatibility fix
 * This header resolves typedef conflicts between GNUstep headers and Windows headers
 */
#ifndef GNUSTEP_FIX_H
#define GNUSTEP_FIX_H

// Fix for mode_t typedef conflict on Windows
// The issue is that sys/types.h defines mode_t as _mode_t (unsigned short)
// but os/generic_win_base.h tries to define it as int
// We prevent the second definition by defining HAVE_MODE_T
#define HAVE_MODE_T

// Now include Foundation which will use the sys/types.h definition
#include <Foundation/Foundation.h>

#endif /* GNUSTEP_FIX_H */
