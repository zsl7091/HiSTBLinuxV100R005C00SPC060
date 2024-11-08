/******************************************************************************

  Copyright (C), 2012-2014, Hisilicon Tech. Co., Ltd.

******************************************************************************
  File Name     : pq_mng_dm.c
  Version       : Initial Draft
  Author        :
  Created       :
  Description   : De-Mosquito

******************************************************************************/
#include <linux/string.h>

#include "hi_math.h"
#include "drv_pq_table.h"
#include "pq_mng_db_alg.h"
#include "pq_mng_db.h"
#include "pq_mng_dm.h"

#define DM_HANDLE_SUPPORT_MAX  16 //must be equal to DB_HANDLE_SUPPORT_MAX

#define DBM_Blend(F,B,An,Ad)  (F*An + B*(Ad-An) + Ad/2)/Ad

extern HI_U32           g_u32DmStr;
extern DetStaticUsing_S g_astDetUsingInf[DM_HANDLE_SUPPORT_MAX];
extern DB_DET_INFO_S    g_astDbDetInf[DM_HANDLE_SUPPORT_MAX];

static const HI_U8 DM_SW_WHT_LUT[5]       = {4, 8, 16, 32, 64};
static const HI_U8 DM_DIR_STR_GAIN_LUT[8] = {8, 8, 8, 8, 8, 8, 6, 5};
static const HI_U8 DM_DIR_STR_LUT[16]     = {8, 8, 8, 8, 8, 8, 8, 6, 1, 1, 0, 0, 0, 0, 0, 0};
static DM_CFG_INFO_S sg_stDmCfgInfo;

DM_INF_S     g_stDmInf[DM_HANDLE_SUPPORT_MAX];
DM_API_REG_S g_stDmApiReg;

//DR 报错机制级别标志
static const HI_U32 PQ_MNG_DR_TRACE_RANGE = HI_LOG_LEVEL_ERROR;

static const HI_U8 g_DmDirStrGainLut[8] = {15, 15, 15, 15, 15, 15, 13, 11};  //u4,[0,15];
static const HI_U8 g_DmDirStrLut[21]    = {1, 2, 4, 6, 8,  10, 12, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};
static const HI_U8 g_DmSwWhtLut[5]      = {4 , 8, 16, 32, 64}; // 8 bit M5,MV410 setting

DM_LimitFlagLut g_LimitFlagLutSD  = {{2, 3, 13, 15}, {1, 2, 2, 2, 2}}; /*mndet*/

/*RM version*/
DM_MMFLimTransBand g_MMFLimTransBand =
{
    {0, 2, 4, 4, 4,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  8,  10, 10, 12, 12, 12, 12, 12, 12, 12, 12, 12},  //low:key
    {0, 2, 4, 6, 6,  8,  8,  9,  9,  9,  10, 10, 10, 10, 12, 12, 12, 12, 12, 12, 14, 14, 16, 16, 16, 16, 18, 18, 18, 18, 18},  //medium
};

DM_Para g_DMParaSD[4] =
{
    {15, 9,  35, 140, 4, 5,     50, 200,   1,  4,   1, 0, 1,      10, 40,  6, 6, 6,    30, 120, 0, 0,  g_MMFLimTransBand.TransBandM },
    {20, 18, 40, 160, 4, 4,     50, 200,   0,  0,   1, 0, 1,      18, 72,  7, 8, 7,    30, 120, 0, 0,  g_MMFLimTransBand.TransBandM },
    {20, 18, 45, 180, 4, 4,     60, 240,  -1,  -4,  1, 1, 1,      20, 80,  7, 8, 7,    30, 120, 0, 0,  g_MMFLimTransBand.TransBandM }, //key
    {25, 18, 50, 200, 4, 4,     60, 240,   0,  0,   1, 1, 0,      25, 100, 8, 8, 8,    30, 120, 0, 0,  g_MMFLimTransBand.TransBandM }
};

static ALG_DM_S sg_stPQDMInfo;

#if defined(PQ_SIZE_UNUSED_CODE)
DM_LimitFlagLut g_LimitFlagLutHD4k = {  {1, 6, 10, 14},   {3, 3, 3, 3, 3}   }; //mndet
#endif

/* 获取DR模块的初始参数值 */
static HI_S32 DM_Init_Value(DM_PARAM_S *pstDRValue)
{
    PQ_CHECK_NULL_PTR_RE_FAIL(pstDRValue);

    pstDRValue->OppAngCtrstT    = 20;  //u8,[0,255];       default value:20
    pstDRValue->MNDirOppCtrstT  = 18;  //u8,[0,255];       default value:18
    pstDRValue->cSWTrsntLT      = 40;  //u8,[0,255];       default value:40
    pstDRValue->cSWTrsntLT10bit = 160; //u10,[0,1023];     default value:160
    pstDRValue->LSWRatio        = 4;   //u3,[2,6];         default value:4
    pstDRValue->LimitLSWRatio   = 4;   //u3,[2,6];         default value:4
    pstDRValue->MMFLR           = 50;  //s9,[-256,255];    default value:50;   MMFLR > MMFSR
    pstDRValue->MMFLR10bit      = 200; //s11,[-1024,1023]; default value:200;  MMFLR10bit > MMFSR10bit
    pstDRValue->MMFSR           = 0;   //s9,[-256,255];    default value:0
    pstDRValue->MMFSR10bit      = 0;   //s11,[-1024,1023]; default value:0
    pstDRValue->MMFlimitEn      = 1;   //u1,[0,1];         default value:1;  1: enable the MMF limit condition; 0: disable

#if 0
    if ((pstDRValue->MMFlimitEn == 1) && ( ((pstDRValue->MMFLR) <= (pstDRValue->MMFSR)) || ((pstDRValue->MMFLR10bit) <= (pstDRValue->MMFSR10bit)) ) )
    {
        pstDRValue->MMFlimitEn = 0;
    }
#endif

    pstDRValue->MMFSet      = 0; //u1,[0,1];  default value:0; 0:MMF[0,3], 1:MMF[1,2]
    pstDRValue->InitValStep = 0; //u2,[0,3];  default value:0

    pstDRValue->LimitT          = 18;  //u8,[0,255];   default value:18
    pstDRValue->LimitT10bit     = 72;  //u10,[0,1023]; default value:72
    pstDRValue->LimResBlendStr1 = 7;   //u4,[0,8];     default value:7;  Res = (HI_U16)DM_Blend(ResLimitT, ResMMF, pReg->LimResBlendStr1, 8);
    pstDRValue->LimResBlendStr2 = 7;   //u4,[0,8];     default value:7;  Res = (HI_U16)DM_Blend(ResLimitT, Pix, pReg->LimResBlendStr2, 8);
    pstDRValue->DirBlendStr     = 7;   //u4,[0,8];     default value:7;  pTxt->DirStr = (HI_U8)DM_Blend(pTxt->DirStr,Gain,pReg->DirBlendStr,8);

    pstDRValue->LWCtrstT        = 30;  //u8,[0,255];   default value:30
    pstDRValue->LWCtrstT10bit   = 120; //u10,[0,1023]; default value:120
    pstDRValue->cSWTrsntST      = 0;   //u8,[0,255];   default value:0
    pstDRValue->cSWTrsntST10bit = 0;   //u10,[0,1023]; default value:0

    pstDRValue->u8GradSubRatio   = 24; //u5,[0,31]; default value:24
    pstDRValue->u8CtrstThresh    = 2;  //u2,[0,3];  default value:2
    pstDRValue->u8OppAngCtrstDiv = 2;  //u2,[1,3];  default value:2
    pstDRValue->u8DmGlobalStr    = 8;  //u4,[0,15]; default value:8

    pstDRValue->pu8DirStrGainLut = g_DmDirStrGainLut;            //u4,[0,8];
    pstDRValue->pu8DirStrLut     = g_DmDirStrLut;                //u4,[0,8];
    pstDRValue->pu8SwWhtLut      = g_DmSwWhtLut;                 //u7,[0,127];
    pstDRValue->pTransBand       = g_MMFLimTransBand.TransBandM; //u6,[0,63];

    return HI_SUCCESS;
}

/* Init DM, Vpss Handle ID = 0, Other Handle Init At Top Layer */
/**
\brief     :初始化DR模块

\attention :只初始化ID为0的实例，其余多实例情况的初始化在上层实现

\param[in] :无

\param[out]:无

\retval    :HI_SUCCESS/HI_FAILURE

*/
HI_S32 PQ_MNG_InitDM(PQ_PARAM_S *pstPqParam, HI_BOOL bParaUseTableDefault)
{
    HI_U32 u32HandleNo = 0;
    DM_PARAM_S stParamValueDR;
    HI_S32 s32Ret = HI_SUCCESS;

    if (HI_TRUE == sg_stPQDMInfo.bInit)
    {
        return HI_SUCCESS;
    }

    sg_stPQDMInfo.bEnable      = HI_FALSE;
    sg_stPQDMInfo.u32Strength  = 50;
    sg_stPQDMInfo.bDemoEnable  = HI_FALSE;

    s32Ret = DM_Init_Value(&stParamValueDR);

    if (HI_SUCCESS != s32Ret)
    {
        HI_ERR_PQ("DM parameter init error!\n");
        sg_stPQDMInfo.bInit = HI_FALSE;

        return HI_FAILURE;
    }

    s32Ret = PQ_TABLE_InitPhyList(u32HandleNo, HI_PQ_MODULE_DM, PQ_SOURCE_MODE_NO, PQ_OUTPUT_MODE_NO);
    if (HI_SUCCESS != s32Ret)
    {
        HI_ERR_PQ("dm InitPhyList error\n");
        sg_stPQDMInfo.bInit = HI_FALSE;

        return HI_FAILURE;
    }

    sg_stPQDMInfo.bInit = HI_TRUE;

    return HI_SUCCESS;
}

/**
\brief     :反初始化DR模块

\attention :无

\param[in] :无

\param[out]:无

\retval    :HI_SUCCESS

*/
HI_S32 PQ_MNG_DeInitDM(HI_VOID)
{
    if (HI_FALSE == sg_stPQDMInfo.bInit)
    {
        return HI_SUCCESS;
    }

    sg_stPQDMInfo.bInit = HI_FALSE;

    return HI_SUCCESS;
}

HI_S32 PQ_MNG_SetDMEn(HI_BOOL bOnOff)
{
    HI_U32  u32HandleNo = 0;
    PQ_SOURCE_MODE_E enSourceMode = PQ_COMM_GetSourceMode();

    if ((enSourceMode > PQ_SOURCE_MODE_HD)
        || (HI_FALSE == PQ_COMM_GetMoudleCtrl(HI_PQ_MODULE_DM, enSourceMode)))
    {
        bOnOff = HI_FALSE;
    }

    sg_stPQDMInfo.bEnable = bOnOff;

    if (REG_TYPE_VPSS == PQ_COMM_GetAlgTypeID(HI_PQ_MODULE_DM))
    {
        for (u32HandleNo = 0; u32HandleNo < VPSS_HANDLE_NUM; u32HandleNo++)
        {
            if (HI_SUCCESS == PQ_HAL_CheckVpssValid(u32HandleNo))
            {
                PQ_HAL_EnableDM(u32HandleNo, bOnOff);
            }
        }
    }
    else
    {
        PQ_HAL_EnableDM(PQ_VDP_LAYER_VID0, bOnOff);
    }

    return HI_SUCCESS;
}

HI_S32 PQ_MNG_GetDMEn(HI_BOOL *pbOnOff)
{
    PQ_CHECK_NULL_PTR_RE_FAIL(pbOnOff);

    *pbOnOff = sg_stPQDMInfo.bEnable;

    return HI_SUCCESS;
}

/* u32Strength: 0~100 */
HI_S32 PQ_MNG_SetDMStrength(HI_U32 u32Strength)
{
    sg_stPQDMInfo.u32Strength = u32Strength;
    g_u32DmStr                = u32Strength * 15 / 100; /* 0~15; Low:4; Mid:8; High:12 */

    return HI_SUCCESS;
}

HI_S32 PQ_MNG_GetDMStrength(HI_U32 *pu32Strength)
{
    PQ_CHECK_NULL_PTR_RE_FAIL(pu32Strength);

    *pu32Strength = sg_stPQDMInfo.u32Strength;

    return HI_SUCCESS;
}

HI_S32 PQ_MNG_SetDMDemoEn(HI_BOOL bOnOff)
{
    HI_U32 u32HandleNo = 0, u32Vid = PQ_VDP_LAYER_VID0;
    HI_S32 s32Ret = HI_SUCCESS;

    sg_stPQDMInfo.bDemoEnable = bOnOff;

    if (REG_TYPE_VPSS == PQ_COMM_GetAlgTypeID(HI_PQ_MODULE_DM))
    {
        for (u32HandleNo = 0; u32HandleNo < VPSS_HANDLE_NUM; ++u32HandleNo)
        {
            if (HI_SUCCESS == PQ_HAL_CheckVpssValid(u32HandleNo))
            {
                s32Ret = PQ_HAL_EnableDMDemo(u32HandleNo, bOnOff);
            }
        }
    }
    else
    {
        s32Ret = PQ_HAL_EnableDMDemo(u32Vid, bOnOff);
    }

    return s32Ret;
}

HI_S32 PQ_MNG_GetDMDemoEn(HI_BOOL *pbOnOff)
{
    PQ_CHECK_NULL_PTR_RE_FAIL(pbOnOff);

    *pbOnOff = sg_stPQDMInfo.bDemoEnable;

    return HI_SUCCESS;
}

static HI_S32 DBM_ReadDmPara(DetStaticUsing_S *pMnDetInf, DB_DET_INFO_S *s_DbDetInf_In, DM_INF_S *pInf)
{
    PQ_CHECK_NULL_PTR_RE_FAIL(pMnDetInf);
    PQ_CHECK_NULL_PTR_RE_FAIL(s_DbDetInf_In);

    pInf->s32LumHBlkSize   = pMnDetInf->UsingSizeHy;

    pInf->s32RateSmthWin   = sg_stDmCfgInfo.u8NumWindow;

    pInf->u32LumWidth      = s_DbDetInf_In->s32LumWidth;
    pInf->u32LumHeight     = s_DbDetInf_In->s32LumHeight;

    /* compatible error: when using info from db detection */
    if (pMnDetInf->UsingSizeHy > 64 || pMnDetInf->UsingSizeHy < 4)
    {
        pInf->s32LumHBlkSize = 0;
    }
    else
    {
        pInf->s32LumHBlkSize = pMnDetInf->UsingSizeHy;
    }
    pInf->s32LumHBlkDetNum = pMnDetInf->UsingHyCounterBorder;

    return HI_SUCCESS;
}


HI_S32 PQ_MNG_DMCfgRefresh(HI_U32 u32HandleNo, HI_U32 u32Height, HI_U32 u32Width, HI_U32 u32FRate)
{
    DM_INF_S *pInf = HI_NULL;
    if (u32HandleNo > DM_HANDLE_SUPPORT_MAX)
    {
        HI_WARN_PQ("u32HandleNo :%d\n", u32HandleNo);
        return HI_SUCCESS;
    }

    pInf = &(g_stDmInf[u32HandleNo]);

    pInf->s32LumHBlkDetNum = 0;
    pInf->s32LumHBlkSize = 0;
    pInf->YWidth = u32Width;
    pInf->pNewRate = pInf->s32RateBuf;

    memset(pInf->s32RateBuf, 0, sizeof(pInf->s32RateBuf));

    pInf->s32RateSmthWin = 6;   //u,[0,16];     default value:6;    number of frames for the average of MNDET data
    pInf->DefaultDMParaFlag = 1; //u,[0,3]; default value:1
    pInf->DMmndetEn = 1; //u,[0,1];   default value:1

    memcpy( &pInf->LimitFlagLutSD, &g_LimitFlagLutSD, sizeof(g_LimitFlagLutSD));
    memcpy( pInf->DMParaSD, g_DMParaSD, sizeof(g_DMParaSD));

    return HI_SUCCESS;
}

HI_S32 PQ_MNG_DMCalcConfigCfg(HI_PQ_DB_CALC_INFO_S *pstDmCalcInfo)
{
    PQ_CHECK_NULL_PTR_RE_FAIL(pstDmCalcInfo);
    if (pstDmCalcInfo->u32HandleNo > DM_HANDLE_SUPPORT_MAX)
    {
        HI_WARN_PQ("u32HandleNo :%d\n", pstDmCalcInfo->u32HandleNo);
        return HI_SUCCESS;
    }

    DBM_ReadDmPara(&g_astDetUsingInf[pstDmCalcInfo->u32HandleNo], &g_astDbDetInf[pstDmCalcInfo->u32HandleNo], &g_stDmInf[pstDmCalcInfo->u32HandleNo]);

    pstDmCalcInfo->u32Protocol = 4; /* 4: mndet mode */
    PQ_MNG_ALG_UpdateDMCfg(&g_stDmInf[pstDmCalcInfo->u32HandleNo], &g_stDmApiReg, (DB_CALC_INFO_S *)pstDmCalcInfo);
    PQ_HAL_SetDMApiReg(pstDmCalcInfo->u32HandleNo, &g_stDmApiReg);

    return HI_SUCCESS;
}

static stPQAlgFuncs stDmFuncs =
{
    .Init               = PQ_MNG_InitDM,
    .DeInit             = PQ_MNG_DeInitDM,
    .SetEnable          = PQ_MNG_SetDMEn,
    .GetEnable          = PQ_MNG_GetDMEn,
    .SetStrength        = PQ_MNG_SetDMStrength,
    .GetStrength        = PQ_MNG_GetDMStrength,
    .SetDemo            = PQ_MNG_SetDMDemoEn,
    .GetDemo            = PQ_MNG_GetDMDemoEn,
    .DMCfgRefresh       = PQ_MNG_DMCfgRefresh,
    .DMCalcConfigCfg    = PQ_MNG_DMCalcConfigCfg,
};

HI_S32 PQ_MNG_RegisterDM(PQ_REG_TYPE_E enType)
{
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = PQ_COMM_AlgRegister(HI_PQ_MODULE_DM, enType, PQ_BIN_ADAPT_MULTIPLE, "dm", HI_NULL, &stDmFuncs);

    return s32Ret;
}

HI_S32 PQ_MNG_UnRegisterDM(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = PQ_COMM_AlgUnRegister(HI_PQ_MODULE_DM);

    return s32Ret;
}

