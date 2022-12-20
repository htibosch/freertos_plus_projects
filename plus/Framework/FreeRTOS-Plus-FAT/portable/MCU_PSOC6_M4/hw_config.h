/* ========================================
 * ========================================
 */

#ifndef _HW_CONFIG_H_
#define _HW_CONFIG_H_

#include "sd_card.h"

sd_card_t * sd_get_by_name( const char * const name );
sd_card_t * sd_get_by_num( size_t num );

#endif

/* [] END OF FILE */
