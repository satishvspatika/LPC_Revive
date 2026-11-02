#ifndef USER_CONFIG_H
#define USER_CONFIG_H

/* =========================================================================
 *  LPC1768 USER CONFIGURATION FILE
 *  Matches configuration parameters from ESP32 AIO9_5.0 architecture.
 * ========================================================================= */

#define CFG_KSNDMC_TRG 1
#define CFG_BIHAR_TRG  2
#define CFG_KSNDMC_TWS 3
#define CFG_SPATIKA_GEN 4

// Select Active Unit Configuration (CFG_KSNDMC_TRG, CFG_BIHAR_TRG, CFG_KSNDMC_TWS)
#ifndef UNIT_CFG
#define UNIT_CFG CFG_KSNDMC_TWS
#endif

#endif // USER_CONFIG_H
