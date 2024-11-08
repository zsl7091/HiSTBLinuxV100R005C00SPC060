#ifndef _CAPTION_CC_XDS_H_
#define _CAPTION_CC_XDS_H_

#include "hi_type.h"

#if defined __cplusplus || defined __cplusplus__
extern "C" {
#endif

/*****************************************************************************
*                    Type Definitions
*****************************************************************************/

/*****************************************************************************
*                    Extern Data Declarations
*****************************************************************************/

 /*****************************************************************************
*                    Extern Function Prototypes
*****************************************************************************/

HI_VOID caption_cc_output_xds_init();

HI_S32 caption_cc_output_xds(HI_U8 u8XDSClass, HI_U8 u8XDSType, HI_U8 *pu8Data, HI_U8 u8DataLen);

#if defined __cplusplus || defined __cplusplus__
}
#endif

#endif
