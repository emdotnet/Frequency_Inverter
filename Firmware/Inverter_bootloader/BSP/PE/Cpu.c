#include "app_cfg.h"
#include "CPU.h"
#include "PE_Error.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Symbols representing MCG modes */
#define MCG_MODE_FBI                    0x00U
#define MCG_MODE_BLPI                   0x01U
#define MCG_MODE_FBE                    0x02U
#define MCG_MODE_PBE                    0x03U
#define MCG_MODE_PEE                    0x04U
static const uint8_t MCGTransitionMatrix[5][5] = {
/* This matrix defines which mode is next in the MCG Mode state diagram in transitioning from the
   current mode to a target mode*/
  {  MCG_MODE_FBI,  MCG_MODE_BLPI,  MCG_MODE_FBE,  MCG_MODE_FBE,  MCG_MODE_FBE }, /* FBI */
  {  MCG_MODE_FBI,  MCG_MODE_BLPI,  MCG_MODE_FBI,  MCG_MODE_FBI,  MCG_MODE_FBI }, /* BLPI */
  {  MCG_MODE_FBI,  MCG_MODE_FBI,  MCG_MODE_FBE,  MCG_MODE_PBE,  MCG_MODE_PBE }, /* FBE */
  {  MCG_MODE_FBE,  MCG_MODE_FBE,  MCG_MODE_FBE,  MCG_MODE_PBE,  MCG_MODE_PEE }, /* PBE */
  {  MCG_MODE_PBE,  MCG_MODE_PBE,  MCG_MODE_PBE,  MCG_MODE_PBE,  MCG_MODE_PEE }  /* PEE */
};

/* Global variables */
volatile uint8_t SR_reg;               /* Current value of the FAULTMASK register */
volatile uint8_t SR_lock = 0x00U;      /* Lock */
static uint8_t ClockConfigurationID = CPU_CLOCK_CONFIG_0; /* Active clock configuration */

/*
** ===================================================================
**     Method      :  Cpu_LDD_SetClockConfiguration (component MK60FN1M0LQ15)
*/
/*!
**     @brief
**         Changes the clock configuration of all LDD components in a 
**         project.
**     @param
**       ClockConfiguration - New CPU clock configuration changed by CPU SetClockConfiguration method.
*/
/* ===================================================================*/
extern void LDD_SetClockConfiguration(LDD_TClockConfiguration ClockConfiguration);

/*
** ===================================================================
**     Method      :  Cpu_SetBASEPRI (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the BASEPRI core register.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
void Cpu_SetBASEPRI(uint32_t Level);

/*
** ===================================================================
**     Method      :  Cpu_MCGAutoTrim (component MK60FN1M0LQ15)
**     Description :
**         This method uses MCG auto trim feature to trim internal
**         reference clock. This method can be used only in a clock
**         configuration which derives its bus clock from external
**         reference clock (<MCG mode> must be one of the following
**         modes - FEE, FBE, BLPE, PEE, PBE) and if value of <Bus clock>
**         is in the range <8; 16>MHz.
**         The slow internal reference clock is trimmed to the value
**         selected by <Slow internal reference clock [kHz]> property. 
**         The fast internal reference clock will be trimmed to value
**         4MHz.
**     Parameters  :
**         NAME            - DESCRIPTION
**         ClockSelect     - Selects which internal
**                           reference clock will be trimmed.
**                           0 ... slow (32kHz) internal reference clock
**                           will be trimmed
**                           > 0 ... fast (4MHz) internal reference
**                           clock will be trimmed
**     Returns     :
**         ---             - Error code
**                           ERR_OK - OK
**                           ERR_SPEED - The method does not work in the
**                           active clock configuration.
**                           ERR_FAILED - Autotrim process failed.
** ===================================================================
*/
LDD_TError Cpu_MCGAutoTrim(uint8_t ClockSelect)
{
  switch (ClockConfigurationID)
  {
  case CPU_CLOCK_CONFIG_1:
    if ( ClockSelect == 0x00U )
    {
      /* Slow internal reference clock */
      MCG_ATCVH = 0x1EU;
      MCG_ATCVL = 0x0AU;
    }
    else
    {
      /* Fast internal reference clock */
      MCG_ATCVH = 0x1FU;
      MCG_ATCVL = 0x80U;
    }
    break;
  default:
    return ERR_SPEED;
  }
  if ( ClockSelect == 0x00U )
  {
    /* MCG_SC: ATME=1,ATMS=0 */
    MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(
                                                     MCG_SC_ATMS_MASK
                                                     )) | (uint8_t)(
                                                                    MCG_SC_ATME_MASK
                                                                    ));                       /* Start trimming of the slow internal reference clock */
  }
  else
  {
    /* MCG_SC: ATME=1,ATMS=1 */
    MCG_SC |= (MCG_SC_ATME_MASK | MCG_SC_ATMS_MASK); /* Start trimming of the fast internal reference clock */
  }
  while ((MCG_SC & MCG_SC_ATME_MASK) != 0x00U) /* Wait until autotrim completes */
  {}
  if ( (MCG_SC & MCG_SC_ATMF_MASK) == 0x00U )
  {
    return ERR_OK;                     /* Trim operation completed successfully */
  }
  else
  {
    return ERR_FAILED;                 /* Trim operation failed */
  }
}

/*
** ===================================================================
**     Method      :  Cpu_GetLLSWakeUpFlags (component MK60FN1M0LQ15)
**     Description :
**         This method returns the current status of the LLWU wake-up
**         flags indicating which wake-up source caused the MCU to exit
**         LLS or VLLSx low power mode.
**         The following predefined constants can be used to determine
**         the wake-up source:
**         LLWU_EXT_PIN0, ... LLWU_EXT_PIN15 - external pin 0 .. 15
**         caused the wake-up
**         LLWU_INT_MODULE0 .. LLWU_INT_MODULE7 - internal module 0..15
**         caused the wake-up.
**     Parameters  : None
**     Returns     :
**         ---             - Returns the current status of the LLWU
**                           wake-up flags indicating which wake-up
**                           source caused the MCU to exit LLS or VLLSx
**                           low power mode.
** ===================================================================
*/
uint32_t Cpu_GetLLSWakeUpFlags(void)
{
  uint32_t Flags;

  Flags = LLWU_F1;
  Flags |= (uint32_t)((uint32_t)LLWU_F2 << 8U);
  Flags |= (uint32_t)((uint32_t)LLWU_F3 << 16U);
  if ( (LLWU_FILT1 & 0x80U) != 0x00U )
  {
    Flags |= LLWU_FILTER1;
  }
  if ( (LLWU_FILT2 & 0x80U) != 0x00U )
  {
    Flags |= LLWU_FILTER2;
  }
  return Flags;
}

static void Cpu_SetMCGModePEE(uint8_t CLKMode);
/*
** ===================================================================
**     Method      :  Cpu_SetMCGModePEE (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to PEE mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/

static void Cpu_SetMCGModePBE(uint8_t CLKMode);
/*
** ===================================================================
**     Method      :  Cpu_SetMCGModePBE (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to PBE mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/

static void Cpu_SetMCGModeFBE(uint8_t CLKMode);
/*
** ===================================================================
**     Method      :  Cpu_SetMCGModeFBE (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to FBE mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/

static void Cpu_SetMCGModeBLPI(uint8_t CLKMode);
/*
** ===================================================================
**     Method      :  Cpu_SetMCGModeBLPI (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to BLPI mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/

static void Cpu_SetMCGModeFBI(uint8_t CLKMode);
/*
** ===================================================================
**     Method      :  Cpu_SetMCGModeFBI (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to FBI mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/

static void Cpu_SetMCG(uint8_t CLKMode);
/*
** ===================================================================
**     Method      :  Cpu_SetMCG (component MK60FN1M0LQ15)
**
**     Description :
**         This method updates the MCG according the requested clock 
**         source setting.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/

static uint8_t Cpu_GetCurrentMCGMode(void);
/*
** ===================================================================
**     Method      :  Cpu_GetCurrentMCGMode (component MK60FN1M0LQ15)
**
**     Description :
**         This method returns the active MCG mode
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/


/*
** ===================================================================
**     Method      :  Cpu_SetMCGModePEE (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to PEE mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
static void Cpu_SetMCGModePEE(uint8_t CLKMode)
{
  switch (CLKMode)
  {
  case 0U:
    /* Switch to PEE Mode */
    /* OSC0_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_ERCLKEN_MASK | OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = OSC_CR_ERCLKEN_MASK;
    /* MCG_C1: CLKS=0,FRDIV=3,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = (MCG_C1_CLKS(0x00) | MCG_C1_FRDIV(0x03) | MCG_C1_IRCLKEN_MASK);
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_IRCS_MASK;
    /* MCG_C11: PLLREFSEL1=0,PLLCLKEN1=0,PLLSTEN1=0,PLLCS=0,??=0,PRDIV1=0 */
    MCG_C11 = MCG_C11_PRDIV1(0x00);
    /* MCG_C12: LOLIE1=0,??=0,CME2=0,VDIV1=0 */
    MCG_C12 = MCG_C12_VDIV1(0x00);
    /* MCG_C5: PLLREFSEL0=0,PLLCLKEN0=0,PLLSTEN0=0,??=0,??=0,PRDIV0=0 */
    MCG_C5 = MCG_C5_PRDIV0(0x00);
    /* MCG_C6: LOLIE0=0,PLLS=1,CME0=0,VDIV0=0x0E */
    MCG_C6 = (MCG_C6_PLLS_MASK | MCG_C6_VDIV0(0x0E));
    while ((MCG_S & 0x0CU) != 0x0CU) /* Wait until output of the PLL is selected */
    {}
    break;
  default:
    break;
  }
}

/*
** ===================================================================
**     Method      :  Cpu_SetMCGModePBE (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to PBE mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
static void Cpu_SetMCGModePBE(uint8_t CLKMode)
{
  switch (CLKMode)
  {
  case 0U:
    /* Switch to PBE Mode */
    /* OSC0_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_ERCLKEN_MASK | OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = OSC_CR_ERCLKEN_MASK;
    /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = (MCG_C1_CLKS(0x02) | MCG_C1_FRDIV(0x03) | MCG_C1_IRCLKEN_MASK);
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_IRCS_MASK;
    /* MCG_C5: PLLREFSEL0=0,PLLCLKEN0=0,PLLSTEN0=0,??=0,??=0,PRDIV0=0 */
    MCG_C5 = MCG_C5_PRDIV0(0x00);
    /* MCG_C6: LOLIE0=0,PLLS=1,CME0=0,VDIV0=0x0E */
    MCG_C6 = (MCG_C6_PLLS_MASK | MCG_C6_VDIV0(0x0E));
    while ((MCG_S & 0x0CU) != 0x08U) /* Wait until external reference clock is selected as MCG output */
    {}
    while ((MCG_S & MCG_S_LOCK0_MASK) == 0x00U) /* Wait until PLL locked */
    {}
    break;
  case 1U:
    /* Switch to PBE Mode */
    /* OSC0_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = 0x00U;
    /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = (MCG_C1_CLKS(0x02) | MCG_C1_FRDIV(0x03) | MCG_C1_IRCLKEN_MASK);
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_IRCS_MASK;
    while ((MCG_S & 0x0CU) != 0x08U) /* Wait until external reference clock is selected as MCG output */
    {}
    break;
  default:
    break;
  }
}

/*
** ===================================================================
**     Method      :  Cpu_SetMCGModeFBE (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to FBE mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
static void Cpu_SetMCGModeFBE(uint8_t CLKMode)
{
  switch (CLKMode)
  {
  case 0U:
    /* Switch to FBE Mode */
    /* MCG_C7: OSCSEL=0 */
    MCG_C7 &= (uint8_t)~(uint8_t)(MCG_C7_OSCSEL_MASK);
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_IRCS_MASK;
    /* OSC0_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_ERCLKEN_MASK | OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = OSC_CR_ERCLKEN_MASK;
    /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = (MCG_C1_CLKS(0x02) | MCG_C1_FRDIV(0x03) | MCG_C1_IRCLKEN_MASK);
    /* MCG_C4: DMX32=0,DRST_DRS=0 */
    MCG_C4 &= (uint8_t)~(uint8_t)((MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x03)));
    /* MCG_C5: PLLREFSEL0=0,PLLCLKEN0=0,PLLSTEN0=0,??=0,??=0,PRDIV0=0 */
    MCG_C5 = MCG_C5_PRDIV0(0x00);
    /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0x0E */
    MCG_C6 = MCG_C6_VDIV0(0x0E);
    /* MCG_C11: PLLREFSEL1=0,PLLCLKEN1=0,PLLSTEN1=0,PLLCS=0,??=0,PRDIV1=0 */
    MCG_C11 = MCG_C11_PRDIV1(0x00);
    /* MCG_C12: LOLIE1=0,??=0,CME2=0,VDIV1=0 */
    MCG_C12 = MCG_C12_VDIV1(0x00);
    while ((MCG_S & MCG_S_OSCINIT0_MASK) == 0x00U) /* Check that the oscillator is running */
    {}
    while ((MCG_S & MCG_S_IREFST_MASK) != 0x00U) /* Check that the source of the FLL reference clock is the external reference clock. */
    {}
    while ((MCG_S & 0x0CU) != 0x08U) /* Wait until external reference clock is selected as MCG output */
    {}
    break;
  case 1U:
    /* Switch to FBE Mode */
    /* MCG_C7: OSCSEL=0 */
    MCG_C7 &= (uint8_t)~(uint8_t)(MCG_C7_OSCSEL_MASK);
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_IRCS_MASK;
    /* OSC0_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = 0x00U;
    /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = (MCG_C1_CLKS(0x02) | MCG_C1_FRDIV(0x03) | MCG_C1_IRCLKEN_MASK);
    /* MCG_C4: DMX32=0,DRST_DRS=0 */
    MCG_C4 &= (uint8_t)~(uint8_t)((MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x03)));
    /* MCG_C5: PLLREFSEL0=0,PLLCLKEN0=0,PLLSTEN0=0,??=0,??=0,PRDIV0=0 */
    MCG_C5 = MCG_C5_PRDIV0(0x00);
    /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0 */
    MCG_C6 = MCG_C6_VDIV0(0x00);
    /* MCG_C11: PLLREFSEL1=0,PLLCLKEN1=0,PLLSTEN1=0,PLLCS=0,??=0,PRDIV1=0 */
    MCG_C11 = MCG_C11_PRDIV1(0x00);
    /* MCG_C12: LOLIE1=0,??=0,CME2=0,VDIV1=0 */
    MCG_C12 = MCG_C12_VDIV1(0x00);
    while ((MCG_S & MCG_S_OSCINIT0_MASK) == 0x00U) /* Check that the oscillator is running */
    {}
    while ((MCG_S & MCG_S_IREFST_MASK) != 0x00U) /* Check that the source of the FLL reference clock is the external reference clock. */
    {}
    while ((MCG_S & 0x0CU) != 0x08U) /* Wait until external reference clock is selected as MCG output */
    {}
    break;
  default:
    break;
  }
}

/*
** ===================================================================
**     Method      :  Cpu_SetMCGModeBLPI (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to BLPI mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
static void Cpu_SetMCGModeBLPI(uint8_t CLKMode)
{
  switch (CLKMode)
  {
  case 1U:
    /* Switch to BLPI Mode */
    /* MCG_C1: CLKS=1,FRDIV=0,IREFS=1,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = MCG_C1_CLKS(0x01) |
             MCG_C1_FRDIV(0x00) |
             MCG_C1_IREFS_MASK |
             MCG_C1_IRCLKEN_MASK;
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=1,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_LP_MASK |
             MCG_C2_IRCS_MASK;
    /* OSC0_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = 0x00U;
    while ((MCG_S & MCG_S_IREFST_MASK) == 0x00U) /* Check that the source of the FLL reference clock is the internal reference clock. */
    {}
    while ((MCG_S & MCG_S_IRCST_MASK) == 0x00U) /* Check that the fast external reference clock is selected. */
    {}
    break;
  default:
    break;
  }
}

/*
** ===================================================================
**     Method      :  Cpu_SetMCGModeFBI (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the MCG to FBI mode.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
static void Cpu_SetMCGModeFBI(uint8_t CLKMode)
{
  switch (CLKMode)
  {
  case 0U:
    /* Switch to FBI Mode */
    /* MCG_C1: CLKS=1,FRDIV=0,IREFS=1,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = MCG_C1_CLKS(0x01) |
             MCG_C1_FRDIV(0x00) |
             MCG_C1_IREFS_MASK |
             MCG_C1_IRCLKEN_MASK;
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_IRCS_MASK;
    /* MCG_C4: DMX32=0,DRST_DRS=0 */
    MCG_C4 &= (uint8_t)~(uint8_t)((MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x03)));
    /* OSC0_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_ERCLKEN_MASK | OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = OSC_CR_ERCLKEN_MASK;
    /* MCG_C7: OSCSEL=0 */
    MCG_C7 &= (uint8_t)~(uint8_t)(MCG_C7_OSCSEL_MASK);
    /* MCG_C5: PLLREFSEL0=0,PLLCLKEN0=0,PLLSTEN0=0,??=0,??=0,PRDIV0=0 */
    MCG_C5 = MCG_C5_PRDIV0(0x00);
    /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0x0E */
    MCG_C6 = MCG_C6_VDIV0(0x0E);
    /* MCG_C11: PLLREFSEL1=0,PLLCLKEN1=0,PLLSTEN1=0,PLLCS=0,??=0,PRDIV1=0 */
    MCG_C11 = MCG_C11_PRDIV1(0x00);
    /* MCG_C12: LOLIE1=0,??=0,CME2=0,VDIV1=0 */
    MCG_C12 = MCG_C12_VDIV1(0x00);       /* 7 */
    while ((MCG_S & MCG_S_IREFST_MASK) == 0x00U) /* Check that the source of the FLL reference clock is the internal reference clock. */
    {}
    while ((MCG_S & 0x0CU) != 0x04U) /* Wait until internal reference clock is selected as MCG output */
    {}
    break;
  case 1U:
    /* Switch to FBI Mode */
    /* MCG_C1: CLKS=1,FRDIV=0,IREFS=1,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = MCG_C1_CLKS(0x01) |
             MCG_C1_FRDIV(0x00) |
             MCG_C1_IREFS_MASK |
             MCG_C1_IRCLKEN_MASK;
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=1,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(0x02) |
             MCG_C2_HGO0_MASK |
             MCG_C2_EREFS0_MASK |
             MCG_C2_IRCS_MASK;
    /* MCG_C4: DMX32=0,DRST_DRS=0 */
    MCG_C4 &= (uint8_t)~(uint8_t)((MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x03)));
    /* OSC0_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
    OSC0_CR = (OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
    /* OSC1_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC1_CR = 0x00U;
    /* MCG_C7: OSCSEL=0 */
    MCG_C7 &= (uint8_t)~(uint8_t)(MCG_C7_OSCSEL_MASK);
    /* MCG_C5: PLLREFSEL0=0,PLLCLKEN0=0,PLLSTEN0=0,??=0,??=0,PRDIV0=0 */
    MCG_C5 = MCG_C5_PRDIV0(0x00);
    /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0 */
    MCG_C6 = MCG_C6_VDIV0(0x00);
    /* MCG_C11: PLLREFSEL1=0,PLLCLKEN1=0,PLLSTEN1=0,PLLCS=0,??=0,PRDIV1=0 */
    MCG_C11 = MCG_C11_PRDIV1(0x00);
    /* MCG_C12: LOLIE1=0,??=0,CME2=0,VDIV1=0 */
    MCG_C12 = MCG_C12_VDIV1(0x00);       /* 7 */
    while ((MCG_S & MCG_S_IREFST_MASK) == 0x00U) /* Check that the source of the FLL reference clock is the internal reference clock. */
    {}
    while ((MCG_S & 0x0CU) != 0x04U) /* Wait until internal reference clock is selected as MCG output */
    {}
    break;
  default:
    break;
  }
}

/*
** ===================================================================
**     Method      :  Cpu_SetMCG (component MK60FN1M0LQ15)
**
**     Description :
**         This method updates the MCG according the requested clock 
**         source setting.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
static void Cpu_SetMCG(uint8_t CLKMode)
{
  uint8_t TargetMCGMode = 0x00U;
  uint8_t NextMCGMode;

  switch (CLKMode)
  {
  case 0U:
    TargetMCGMode = MCG_MODE_PEE;
    break;
  case 1U:
    TargetMCGMode = MCG_MODE_BLPI;
    break;
  default:
    break;
  }
  NextMCGMode = Cpu_GetCurrentMCGMode(); /* Identify the currently active MCG mode */
  do
  {
    NextMCGMode = MCGTransitionMatrix[NextMCGMode][TargetMCGMode]; /* Get the next MCG mode on the path to the target MCG mode */
    switch (NextMCGMode)             /* Set the next MCG mode on the path to the target MCG mode */
    {
    case MCG_MODE_FBI:
      Cpu_SetMCGModeFBI(CLKMode);
      break;
    case MCG_MODE_BLPI:
      Cpu_SetMCGModeBLPI(CLKMode);
      break;
    case MCG_MODE_FBE:
      Cpu_SetMCGModeFBE(CLKMode);
      break;
    case MCG_MODE_PBE:
      Cpu_SetMCGModePBE(CLKMode);
      break;
    case MCG_MODE_PEE:
      Cpu_SetMCGModePEE(CLKMode);
      break;
    default:
      break;
    }
  }
  while (TargetMCGMode != NextMCGMode); /* Loop until the target MCG mode is set */
}

/*
** ===================================================================
**     Method      :  Cpu_GetCurrentMCGMode (component MK60FN1M0LQ15)
**
**     Description :
**         This method returns the active MCG mode
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
uint8_t Cpu_GetCurrentMCGMode(void)
{
  switch (MCG_C1  & MCG_C1_CLKS_MASK)
  {
  case  0x00U:
    return MCG_MODE_PEE;
  case 0x40U:
    /* Internal reference clock is selected */
    if ( (MCG_C2 & MCG_C2_LP_MASK) == 0x00U )
    {
      /* Low power mode is disabled */
      return MCG_MODE_FBI;
    }
    else
    {
      /* Low power mode is enabled */
      return MCG_MODE_BLPI;
    }
  case 0x80U:
    if ( (MCG_C6 & MCG_C6_PLLS_MASK) == 0x00U )
    {
      /* FLL is selected */
      return MCG_MODE_FBE;
    }
    else
    {
      /* PLL is selected */
      return MCG_MODE_PBE;
    }
  default:
    return 0x00U;
  }
}

/*
** ===================================================================
**     Method      :  Cpu_SetClockConfiguration (component MK60FN1M0LQ15)
**     Description :
**         Calling of this method will cause the clock configuration
**         change and reconfiguration of all components according to
**         the requested clock configuration setting.
**     Parameters  :
**         NAME            - DESCRIPTION
**         ModeID          - Clock configuration identifier
**     Returns     :
**         ---             - ERR_OK - OK.
**                           ERR_RANGE - Mode parameter out of range
** ===================================================================
*/
LDD_TError Cpu_SetClockConfiguration(LDD_TClockConfiguration ModeID)
{
  if ( ModeID > 0x02U )
  {
    return ERR_RANGE;                  /* Undefined clock configuration requested requested */
  }
  switch (ModeID)
  {
  case CPU_CLOCK_CONFIG_0:
    if ( ClockConfigurationID == 2U )
    {
      /* Clock configuration 0 and clock configuration 2 use different clock configuration */
      /* MCG_C6: CME0=0 */
      MCG_C6 &= (uint8_t)~(uint8_t)(MCG_C6_CME0_MASK); /* Disable the clock monitor */
      /* SIM_CLKDIV1: OUTDIV1=1,OUTDIV2=3,OUTDIV3=7,OUTDIV4=7,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
      SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0x01) |
                    SIM_CLKDIV1_OUTDIV2(0x03) |
                    SIM_CLKDIV1_OUTDIV3(0x07) |
                    SIM_CLKDIV1_OUTDIV4(0x07); /* Set the system prescalers to safe value */
      if ( (MCG_C2 & MCG_C2_IRCS_MASK) == 0x00U )
      {
        /* MCG_SC: FCRDIV=1 */
        MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(MCG_SC_FCRDIV(0x06))) | (uint8_t)(MCG_SC_FCRDIV(0x01)));
      }
      else
      {
        /* MCG_C2: IRCS=0 */
        MCG_C2 &= (uint8_t)~(uint8_t)(MCG_C2_IRCS_MASK);
        /* MCG_SC: FCRDIV=1 */
        MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(MCG_SC_FCRDIV(0x06))) | (uint8_t)(MCG_SC_FCRDIV(0x01)));
        /* MCG_C2: IRCS=1 */
        MCG_C2 |= MCG_C2_IRCS_MASK;
      }
      Cpu_SetMCG(0U);                /* Update clock source setting */
      /* MCG_C6: CME0=1 */
      MCG_C6 |= MCG_C6_CME0_MASK;    /* Enable the clock monitor */
    }
    /* SIM_CLKDIV1: OUTDIV1=0,OUTDIV2=1,OUTDIV3=2,OUTDIV4=5,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0x00) |
                  SIM_CLKDIV1_OUTDIV2(0x01) |
                  SIM_CLKDIV1_OUTDIV3(0x02) |
                  SIM_CLKDIV1_OUTDIV4(0x05); /* Update system prescalers */
    /* SIM_SOPT2: PLLFLLSEL=1 */
    SIM_SOPT2 = (uint32_t)((SIM_SOPT2 & (uint32_t)~(uint32_t)(SIM_SOPT2_PLLFLLSEL(0x02))) | (uint32_t)(SIM_SOPT2_PLLFLLSEL(0x01)));                  /* Select PLL 0 as a clock source for various peripherals */
    /* SIM_SOPT1: OSC32KSEL=1 */
    SIM_SOPT1 |= SIM_SOPT1_OSC32KSEL_MASK; /* RTC oscillator drives 32 kHz clock for various peripherals */
    break;
  case CPU_CLOCK_CONFIG_1:
    if ( ClockConfigurationID == 2U )
    {
      /* Clock configuration 1 and clock configuration 2 use different clock configuration */
      /* MCG_C6: CME0=0 */
      MCG_C6 &= (uint8_t)~(uint8_t)(MCG_C6_CME0_MASK); /* Disable the clock monitor */
      /* SIM_CLKDIV1: OUTDIV1=1,OUTDIV2=3,OUTDIV3=7,OUTDIV4=7,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
      SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0x01) |
                    SIM_CLKDIV1_OUTDIV2(0x03) |
                    SIM_CLKDIV1_OUTDIV3(0x07) |
                    SIM_CLKDIV1_OUTDIV4(0x07); /* Set the system prescalers to safe value */
      if ( (MCG_C2 & MCG_C2_IRCS_MASK) == 0x00U )
      {
        /* MCG_SC: FCRDIV=1 */
        MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(MCG_SC_FCRDIV(0x06))) | (uint8_t)(MCG_SC_FCRDIV(0x01)));
      }
      else
      {
        /* MCG_C2: IRCS=0 */
        MCG_C2 &= (uint8_t)~(uint8_t)(MCG_C2_IRCS_MASK);
        /* MCG_SC: FCRDIV=1 */
        MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(MCG_SC_FCRDIV(0x06))) | (uint8_t)(MCG_SC_FCRDIV(0x01)));
        /* MCG_C2: IRCS=1 */
        MCG_C2 |= MCG_C2_IRCS_MASK;
      }
      Cpu_SetMCG(0U);                /* Update clock source setting */
      /* MCG_C6: CME0=1 */
      MCG_C6 |= MCG_C6_CME0_MASK;    /* Enable the clock monitor */
    }
    /* SIM_CLKDIV1: OUTDIV1=9,OUTDIV2=9,OUTDIV3=9,OUTDIV4=9,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0x09) |
                  SIM_CLKDIV1_OUTDIV2(0x09) |
                  SIM_CLKDIV1_OUTDIV3(0x09) |
                  SIM_CLKDIV1_OUTDIV4(0x09); /* Update system prescalers */
    /* SIM_SOPT2: PLLFLLSEL=1 */
    SIM_SOPT2 = (uint32_t)((SIM_SOPT2 & (uint32_t)~(uint32_t)(SIM_SOPT2_PLLFLLSEL(0x02))) | (uint32_t)(SIM_SOPT2_PLLFLLSEL(0x01)));                  /* Select PLL 0 as a clock source for various peripherals */
    /* SIM_SOPT1: OSC32KSEL=1 */
    SIM_SOPT1 |= SIM_SOPT1_OSC32KSEL_MASK; /* RTC oscillator drives 32 kHz clock for various peripherals */
    break;
  case CPU_CLOCK_CONFIG_2:
    /* MCG_C6: CME0=0 */
    MCG_C6 &= (uint8_t)~(uint8_t)(MCG_C6_CME0_MASK); /* Disable the clock monitor */
    /* SIM_CLKDIV1: OUTDIV1=1,OUTDIV2=3,OUTDIV3=7,OUTDIV4=7,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0x01) |
                  SIM_CLKDIV1_OUTDIV2(0x03) |
                  SIM_CLKDIV1_OUTDIV3(0x07) |
                  SIM_CLKDIV1_OUTDIV4(0x07); /* Set the system prescalers to safe value */
    if ( (MCG_C2 & MCG_C2_IRCS_MASK) == 0x00U )
    {
      /* MCG_SC: FCRDIV=1 */
      MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(MCG_SC_FCRDIV(0x06))) | (uint8_t)(MCG_SC_FCRDIV(0x01)));
    }
    else
    {
      /* MCG_C2: IRCS=0 */
      MCG_C2 &= (uint8_t)~(uint8_t)(MCG_C2_IRCS_MASK);
      /* MCG_SC: FCRDIV=1 */
      MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(MCG_SC_FCRDIV(0x06))) | (uint8_t)(MCG_SC_FCRDIV(0x01)));
      /* MCG_C2: IRCS=1 */
      MCG_C2 |= MCG_C2_IRCS_MASK;
    }
    Cpu_SetMCG(1U);                  /* Update clock source setting */
    /* SIM_CLKDIV1: OUTDIV1=0,OUTDIV2=0,OUTDIV3=0,OUTDIV4=3,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0x00) |
                  SIM_CLKDIV1_OUTDIV2(0x00) |
                  SIM_CLKDIV1_OUTDIV3(0x00) |
                  SIM_CLKDIV1_OUTDIV4(0x03); /* Update system prescalers */
    /* SIM_SOPT2: PLLFLLSEL=3 */
    SIM_SOPT2 |= SIM_SOPT2_PLLFLLSEL(0x03); /* Select Core clock as a clock source for various peripherals */
    /* SIM_SOPT1: OSC32KSEL=0 */
    SIM_SOPT1 &= (uint32_t)~(uint32_t)(SIM_SOPT1_OSC32KSEL_MASK); /* System oscillator drives 32 kHz clock for various peripherals */
    break;
  default:
    break;
  }
  //LDD_SetClockConfiguration(ModeID);   /* Call all LDD components to update the clock configuration */
  ClockConfigurationID = ModeID;       /* Store clock configuration identifier */
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  Cpu_GetClockConfiguration (component MK60FN1M0LQ15)
**     Description :
**         Returns the active clock configuration identifier. The
**         method is enabled only if more than one clock configuration
**         is enabled in the component.
**     Parameters  : None
**     Returns     :
**         ---             - Active clock configuration identifier
** ===================================================================
*/
uint8_t Cpu_GetClockConfiguration(void)
{
  return ClockConfigurationID;         /* Return the actual clock configuration identifier */
}

/*
** ===================================================================
**     Method      :  Cpu_SetOperationMode (component MK60FN1M0LQ15)
**     Description :
**         This method requests to change the component's operation
**         mode (RUN, WAIT, SLEEP, STOP). The target operation mode
**         will be entered immediately. 
**         See <Operation mode settings> for further details of the
**         operation modes mapping to low power modes of the cpu.
**     Parameters  :
**         NAME            - DESCRIPTION
**         OperationMode   - Requested driver
**                           operation mode
**         ModeChangeCallback - Callback to
**                           notify the upper layer once a mode has been
**                           changed. Parameter is ignored, only for
**                           compatibility of API with other components.
**       * ModeChangeCallbackParamPtr 
**                           - Pointer to callback parameter to notify
**                           the upper layer once a mode has been
**                           changed. Parameter is ignored, only for
**                           compatibility of API with other components.
**     Returns     :
**         ---             - Error code
**                           ERR_OK - OK
**                           ERR_PARAM_MODE - Invalid operation mode
** ===================================================================
*/
LDD_TError Cpu_SetOperationMode(LDD_TDriverOperationMode OperationMode, LDD_TCallback ModeChangeCallback, LDD_TCallbackParam *ModeChangeCallbackParamPtr)
{
  (void)ModeChangeCallback;           /* Parameter is not used, suppress unused argument warning */
  (void)ModeChangeCallbackParamPtr;   /* Parameter is not used, suppress unused argument warning */
  switch (OperationMode)
  {
  case DOM_RUN:
    /* SCB_SCR: SLEEPDEEP=0,SLEEPONEXIT=0 */
    SCB_SCR &= (uint32_t)~(uint32_t)(
                                     SCB_SCR_SLEEPDEEP_MASK |
                                       SCB_SCR_SLEEPONEXIT_MASK
                                     );
    if  ( ClockConfigurationID != 2U )
    {
      if ( (MCG_S & MCG_S_CLKST_MASK) != MCG_S_CLKST(3) ) /* If in PBE mode, switch to PEE. PEE to PBE transition was caused by wakeup from low power mode. */
      {
        /* MCG_C1: CLKS=0,IREFS=0 */
        MCG_C1 &= (uint8_t)~(uint8_t)((MCG_C1_CLKS(0x03) | MCG_C1_IREFS_MASK));
        while ((MCG_S & MCG_S_LOCK0_MASK) == 0x00U) /* Wait for PLL lock */
        {}
      }
    }
    break;
  case DOM_WAIT:
    /* SCB_SCR: SLEEPDEEP=0 */
    SCB_SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPDEEP_MASK);
    /* SCB_SCR: SLEEPONEXIT=0 */
    SCB_SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_MASK);
    PE_WFI();
    break;
  case DOM_SLEEP:
    /* SCB_SCR: SLEEPDEEP=1 */
    SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;
    /* SMC_PMCTRL: STOPM=0 */
    SMC_PMCTRL &= (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x07));
    (void)(SMC_PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
    /* SCB_SCR: SLEEPONEXIT=1 */
    SCB_SCR |= SCB_SCR_SLEEPONEXIT_MASK;
    PE_WFI();
    break;
  case DOM_STOP:
    /* Clear LLWU flags */
    /* LLWU_F1: WUF7=1,WUF6=1,WUF5=1,WUF4=1,WUF3=1,WUF2=1,WUF1=1,WUF0=1 */
    LLWU_F1 = LLWU_F1_WUF7_MASK |
              LLWU_F1_WUF6_MASK |
              LLWU_F1_WUF5_MASK |
              LLWU_F1_WUF4_MASK |
              LLWU_F1_WUF3_MASK |
              LLWU_F1_WUF2_MASK |
              LLWU_F1_WUF1_MASK |
              LLWU_F1_WUF0_MASK;
    /* LLWU_F2: WUF15=1,WUF14=1,WUF13=1,WUF12=1,WUF11=1,WUF10=1,WUF9=1,WUF8=1 */
    LLWU_F2 = LLWU_F2_WUF15_MASK |
              LLWU_F2_WUF14_MASK |
              LLWU_F2_WUF13_MASK |
              LLWU_F2_WUF12_MASK |
              LLWU_F2_WUF11_MASK |
              LLWU_F2_WUF10_MASK |
              LLWU_F2_WUF9_MASK |
              LLWU_F2_WUF8_MASK;
    /* LLWU_F3: MWUF7=1,MWUF6=1,MWUF5=1,MWUF4=1,MWUF3=1,MWUF2=1,MWUF1=1,MWUF0=1 */
    LLWU_F3 = LLWU_F3_MWUF7_MASK |
              LLWU_F3_MWUF6_MASK |
              LLWU_F3_MWUF5_MASK |
              LLWU_F3_MWUF4_MASK |
              LLWU_F3_MWUF3_MASK |
              LLWU_F3_MWUF2_MASK |
              LLWU_F3_MWUF1_MASK |
              LLWU_F3_MWUF0_MASK;
    /* LLWU_FILT1: FILTF=1 */
    LLWU_FILT1 |= LLWU_FILT1_FILTF_MASK;
    /* LLWU_FILT2: FILTF=1 */
    LLWU_FILT2 |= LLWU_FILT2_FILTF_MASK;
    /* SCB_SCR: SLEEPONEXIT=0 */
    SCB_SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_MASK);
    /* SCB_SCR: SLEEPDEEP=1 */
    SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;
    /* SMC_PMCTRL: STOPM=3 */
    SMC_PMCTRL = (uint8_t)((SMC_PMCTRL & (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x04))) | (uint8_t)(SMC_PMCTRL_STOPM(0x03)));
    (void)(SMC_PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
    PE_WFI();
    break;
  default:
    return ERR_PARAM_MODE;
  }
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  Cpu_EnableInt (component MK60FN1M0LQ15)
**     Description :
**         Enables all maskable interrupts.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void Cpu_EnableInt(void)
{
  __EI();
}

/*
** ===================================================================
**     Method      :  Cpu_DisableInt (component MK60FN1M0LQ15)
**     Description :
**         Disables all maskable interrupts.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void Cpu_DisableInt(void)
{
  __DI();
}


extern INT32U ___VECTOR_RAM[];         //Get vector table that was copied to RAM
//-------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------
void Init_cpu(void)
{
  SCB_MemMapPtr  SCB  = SystemControl_BASE_PTR;
  NVIC_MemMapPtr NVIC = NVIC_BASE_PTR;
  SIM_MemMapPtr  SIM  = SIM_BASE_PTR;
  RTC_MemMapPtr  RTC  = RTC_BASE_PTR;
  MCG_MemMapPtr  MCG  = MCG_BASE_PTR;
  PIT_MemMapPtr  PIT  = PIT_BASE_PTR;
  FMC_MemMapPtr  FMC  = FMC_BASE_PTR;
  CRC_MemMapPtr  CRC  = CRC_BASE_PTR;      

  MPU_BASE_PTR->CESR = 0;    // 0 MPU is disabled. All accesses from all bus masters are allowed.

  SCB->VTOR = (INT32U)___VECTOR_RAM;



  //--------------------------------------------------------------------------------------------------------------------------------------
  SIM->SCGC6 |= BIT(29); // RTC | RTC clock gate control | 1 Clock is enabled.
  if ( (RTC->CR & BIT(8)) == 0u ) // If 0, 32.768 kHz oscillator is disabled.
  {
    RTC->CR = 0
              + LSHIFT(0x00, 13) // SC2P | Oscillator 2pF load configure  | 0 Disable the load.
              + LSHIFT(0x00, 12) // SC4P | Oscillator 4pF load configure  | 0 Disable the load.
              + LSHIFT(0x00, 11) // SC8P | Oscillator 8pF load configure  | 0 Disable the load.
              + LSHIFT(0x00, 10) // SC16P| Oscillator 16pF load configure | 0 Disable the load.
              + LSHIFT(0x01, 9)  // CLKO | Clock Output                   | 1 The 32kHz clock is not output to other peripherals
              + LSHIFT(0x01, 8)  // OSCE | Oscillator Enable              | 1 32.768 kHz oscillator is enabled.
              + LSHIFT(0x00, 3)  // UM   | Update Mode                    | 0 Registers cannot be written when locked.
              + LSHIFT(0x00, 2)  // SUP  | Supervisor Access              | 0 Non-supervisor mode write accesses are not supported and generate a bus error.
              + LSHIFT(0x00, 1)  // WPE  | Wakeup Pin Enable              | 0 Wakeup pin is disabled.
              + LSHIFT(0x00, 0)  // SWR  | Software Reset                 | 0 No effect
    ;
  }




  //--------------------------------------------------------------------------------------------------------------------------------------
  // Загрузка регистров из области чипа с заводскими установками
  if ( *((uint8_t *)0x03FFU) != 0xFFU )
  {
    MCG->C3 = *((uint8_t *)0x03FFU);
    MCG->C4 = (MCG_C4 & 0xE0U) | ((*((uint8_t *)0x03FEU)) & 0x1FU);
  }

  //--------------------------------------------------------------------------------------------------------------------------------------
  SIM->CLKDIV1 = 0
                 + LSHIFT(0x00, 28) // OUTDIV1 | Divide value for the core/system clock                                  | 0000 Divide-by-1.
                 + LSHIFT(0x01, 24) // OUTDIV2 | Divide value for the peripheral clock                                   | 0001 Divide-by-2.
                 + LSHIFT(0x02, 20) // OUTDIV3 | Divide value for the FlexBus clock driven to the external pin (FB_CLK). | 0010 Divide-by-3.
                 + LSHIFT(0x05, 16) // OUTDIV4 | Divide value for the flash clock                                        | 0101 Divide-by-6.
  ;

  //
  SIM->SOPT2 = 0
               + LSHIFT(0x01, 30) // NFCSRC      | NFC Flash clock source select         | 01 MCGPLL0CLK
               + LSHIFT(0x00, 28) // ESDHCSRC    | ESDHC perclk source select            | 00 Core/system clock
               + LSHIFT(0x01, 22) // USBFSRC     | USB FS clock source select            | 01 MCGPLL0CLK
               + LSHIFT(0x00, 20) // TIMESRC     | Ethernet timestamp clock source select| 00 System platform clock
               + LSHIFT(0x01, 18) // USBF_CLKSEL | USB FS clock select                   | 1 Clock divider USB FS clock
               + LSHIFT(0x01, 16) // PLLFLLSEL   | PLL/FLL clock select                  | 01 MCGPLL0CLK !!!
               + LSHIFT(0x00, 15) // NFC_CLKSEL  | NFC Flash clock select                | 0 Clock divider NFC clock
               + LSHIFT(0x01, 12) // TRACECLKSEL | Debug trace clock select              | 0 MCGCLKOUT
               + LSHIFT(0x00, 11) // CMTUARTPAD  | CMT/UART pad drive strength           | 0 Single-pad drive strength for CMT IRO or UART0_TXD.
               + LSHIFT(0x00, 8)  // FBSL        | Flexbus security level                | 00 All off-chip accesses (op code and data) via the FlexBus are disallowed.
               + LSHIFT(0x02, 5)  // CLKOUTSEL   | Clock out select                      | 010 Flash ungated clock
               + LSHIFT(0x00, 4)  // RTCCLKOUTSEL| RTC clock out select                  | 0 RTC 1 Hz clock drives RTC CLKOUT.
               + LSHIFT(0x01, 2)  // USBHSRC     | USB HS clock source select            | 01 MCGPLL0CLK
  ;
  SIM->SOPT1 = 0
               + LSHIFT(0x00, 31) // USBREGEN  | USB voltage regulator enable
               + LSHIFT(0x00, 30) // USBSSTBY  | UUSB voltage regulator in standby mode during Stop, VLPS, LLS or VLLS
               + LSHIFT(0x00, 29) // USBVSTBY  | USB voltage regulator in standby mode during VLPR or VLPW
               + LSHIFT(0x01, 19) // OSC32KSEL | 32 kHz oscillator clock select
  ;


  SIM->SCGC1 |= BIT(5); // OSC1 | OSC1 clock gate control | 1 Clock is enabled.


  /* PORTA_PCR18: ISF=0,MUX=0 */
  //PORTA_PCR18 &= (uint32_t)~(uint32_t)((PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x07)));
  /* PORTA_PCR19: ISF=0,MUX=0 */
  //PORTA_PCR19 &= (uint32_t)~(uint32_t)((PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x07)));


  /* Switch to FBE Mode */
  //--------------------------------------------------------------------------------------------------------------------------------------

  MCG->C7 = 0; // OSCSEL | MCG OSC Clock Select | 0 Selects System Oscillator (OSCCLK).

  MCG->C2 = 0
            + LSHIFT(0x00, 7) // LOCRE0 | Loss of Clock Reset Enable     | 0 Interrupt request is generated on a loss of OSC0 external reference clock.
            + LSHIFT(0x02, 4) // RANGE0 | Frequency Range Select         | 1X Encoding 2 — Very high frequency range selected for the crystal oscillator .
            + LSHIFT(0x01, 3) // HGO0   | High Gain Oscillator Select    | 1 Configure crystal oscillator for high-gain operation.
            + LSHIFT(0x01, 2) // EREFS0 | External Reference Select      | 1 Oscillator requested.
            + LSHIFT(0x00, 1) // LP     | Low Power Select               | 0 FLL (or PLL) is not disabled in bypass modes.
            + LSHIFT(0x01, 0) // IRCS   | Internal Reference Clock Select| 1 Fast internal reference clock selected.
  ;



  /* OSC0_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=1,SC8P=1,SC16P=0 */
  OSC0_CR = (OSC_CR_ERCLKEN_MASK | OSC_CR_SC4P_MASK | OSC_CR_SC8P_MASK);
  /* OSC1_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
  OSC1_CR = OSC_CR_ERCLKEN_MASK;
  /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
  MCG_C1 = (MCG_C1_CLKS(0x02) | MCG_C1_FRDIV(0x03) | MCG_C1_IRCLKEN_MASK);
  /* MCG_C4: DMX32=0,DRST_DRS=0 */
  MCG_C4 &= (uint8_t)~(uint8_t)((MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x03)));
  /* MCG_C5: PLLREFSEL0=0,PLLCLKEN0=0,PLLSTEN0=0,??=0,??=0,PRDIV0=0 */
  MCG_C5 = MCG_C5_PRDIV0(0x00);
  /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0x0E */
  MCG_C6 = MCG_C6_VDIV0(0x0E);
  /* MCG_C11: PLLREFSEL1=0,PLLCLKEN1=0,PLLSTEN1=0,PLLCS=0,??=0,PRDIV1=0 */
  MCG_C11 = MCG_C11_PRDIV1(0x00);
  /* MCG_C12: LOLIE1=0,??=0,CME2=0,VDIV1=0 */
  MCG_C12 = MCG_C12_VDIV1(0x00);
  while ((MCG_S & MCG_S_OSCINIT0_MASK) == 0x00U) /* Check that the oscillator is running */
  {}
  while ((MCG_S & MCG_S_IREFST_MASK) != 0x00U) /* Check that the source of the FLL reference clock is the external reference clock. */
  {}
  while ((MCG_S & 0x0CU) != 0x08U)    /* Wait until external reference clock is selected as MCG output */
  {}
  /* Switch to PBE Mode */
  /* MCG_C6: LOLIE0=0,PLLS=1,CME0=0,VDIV0=0x0E */
  MCG_C6 = (MCG_C6_PLLS_MASK | MCG_C6_VDIV0(0x0E));
  while ((MCG_S & 0x0CU) != 0x08U)    /* Wait until external reference clock is selected as MCG output */
  {}
  while ((MCG_S & MCG_S_LOCK0_MASK) == 0x00U) /* Wait until PLL locked */
  {}
  /* Switch to PEE Mode */
  /* MCG_C1: CLKS=0,FRDIV=3,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
  MCG_C1 = (MCG_C1_CLKS(0x00) | MCG_C1_FRDIV(0x03) | MCG_C1_IRCLKEN_MASK);
  while ((MCG_S & 0x0CU) != 0x0CU)    /* Wait until output of the PLL is selected */
  {}
  /* MCG_C6: CME0=1 */
  MCG_C6 |= MCG_C6_CME0_MASK;          /* Enable the clock monitor */


  //--------------------------------------------------------------------------------------------------------------------------------------
  // Настройка частоты тактирования USB FS. Частота должна быть точно выставлена на 48 МГц
  // При частоте шины 120 МГц делитель равен 2.5  Формула Divider output clock = Divider input clock ? [(USBFSFRAC+1) / (USBFSDIV+1)]
  SIM->CLKDIV2 = 0
                 + LSHIFT(0x00, 9) // USBHSDIV  | USB HS clock divider divisor  |
                 + LSHIFT(0x00, 8) // USBHSFRAC | USB HS clock divider fraction |
                 + LSHIFT(0x04, 1) // USBFSDIV  | USB FS clock divider divisor  | = (4+1) =5
                 + LSHIFT(0x01, 0) // USBFSFRAC | USB FS clock divider fraction | = (1+1) =2 , 120*(2/5) = 48 МГц
  ;
  SIM->SOPT1CFG |= BIT(24);  // URWE | USB voltage regulator enable write enable | 1 SOPT1[USBREGEN] can be written.
  SIM->SOPT1    &= ~BIT(31); // USBREGEN | Controls whether the USB voltage regulator is enabled | 0 USB voltage regulator is disabled.
  SIM->SCGC4    |= BIT(18);  // USBFS | USB FS clock gate control | 1 Clock is enabled.
                             //--------------------------------------------------------------------------------------------------------------------------------------



  //--------------------------------------------------------------------------------------------------------------------------------------
  // Установки  Reset Control Module (RCM)
  RCM_BASE_PTR->RPFW = 0
                       + LSHIFT(0x1F, 0) // RSTFLTSEL | Selects the reset pin bus clock filter width.| 11111 Bus clock filter count is 32
  ;
  RCM_BASE_PTR->RPFC = 0
                       + LSHIFT(0x00, 2) // RSTFLTSS  | Selects how the reset pin filter is enabled in STOP and VLPS modes. | 0 All filtering disabled
                       + LSHIFT(0x01, 0) // RSTFLTSRW | Selects how the reset pin filter is enabled in run and wait modes.  | 01 Bus clock filter enabled for normal operation
  ;

  // Установки Power Management Controller (PMC)
  PMC_BASE_PTR->REGSC = 0
                        + LSHIFT(0x00, 3) // ACKISO | Acknowledge Isolation | 0 Peripherals and I/O pads are in normal run state|
                                          //          Writing one to this bit when it is set releases the I/O pads and certain peripherals to their normal run mode state
                        + LSHIFT(0x00, 0) // BGBE   | Bandgap Buffer Enable | 0 Bandgap buffer not enabled
  ;


  PMC_BASE_PTR->LVDSC1 = 0
                         + LSHIFT(0x01, 6) // LVDACK | Low-Voltage Detect Acknowledge     | This write-only bit is used to acknowledge low voltage detection errors (write 1 to clear LVDF). Reads always return 0.
                         + LSHIFT(0x00, 5) // LVDIE  | Low-Voltage Detect Interrupt Enable| 0 Hardware interrupt disabled (use polling)
                         + LSHIFT(0x01, 4) // LVDRE  | Low-Voltage Detect Reset Enable    | 1 Force an MCU reset when LVDF = 1
                         + LSHIFT(0x00, 0) // LVDV   | Low-Voltage Detect Voltage Select  | 00 Low trip point selected (V LVD = V LVDL )
  ;

  PMC_BASE_PTR->LVDSC2 = 0
                         + LSHIFT(0x01, 6) // LVWACK | Low-Voltage Warning Acknowledge      |
                         + LSHIFT(0x00, 5) // LVWIE  | Low-Voltage Warning Interrupt Enable | 0 Hardware interrupt disabled (use polling)
                         + LSHIFT(0x00, 0) // LVWV   | Low-Voltage Warning Voltage Select   | 00 Low trip point selected (V LVW = V LVW1 )
  ;

  //--------------------------------------------------------------------------------------------------------------------------------------
  // Инициализация Periodic Interrupt timer
  SIM->SCGC6 |= BIT(23); // PIT | PIT clock gate control | 1 Clock is enabled.

  PIT->MCR  = 0
              + LSHIFT(0x00, 1) // MDIS | Module Disable | 0 Clock for PIT Timers is enabled.
              + LSHIFT(0x01, 0) // FRZ  | Freeze         | 1 Timers are stopped in debug mode.
  ;

  //--------------------------------------------------------------------------------------------------------------------------------------
  // Инициализация Flash Access Protection Register для разрешения доступа к Flash по DMA и от прочих мастеров
  FMC->PFAPR = 0
               + LSHIFT(0x01, 23) // M7PFD     | 1 Prefetching for this master is disabled.   (Ethernet)
               + LSHIFT(0x01, 22) // M6PFD     | 1 Prefetching for this master is disabled.   (USB HS)
               + LSHIFT(0x01, 21) // M5PFD     | 1 Prefetching for this master is disabled.   () 
               + LSHIFT(0x01, 20) // M4PFD     | 1 Prefetching for this master is disabled.   ()
               + LSHIFT(0x00, 19) // M3PFD     | 0 Prefetching for this master is enabled.    (SDHC, NFC, USB FS)
               + LSHIFT(0x00, 18) // M2PFD     | 0 Prefetching for this master is enabled.    (DMA, EzPort) 
               + LSHIFT(0x00, 17) // M1PFD     | 0 Prefetching for this master is enabled.    (ARM core system bus)
               + LSHIFT(0x00, 16) // M0PFD     | 0 Prefetching for this master is enabled.    (ARM core code bus)
               + LSHIFT(0x00, 14) // M7AP[1:0] | 00 No access may be performed by this master.
               + LSHIFT(0x00, 12) // M6AP[1:0] | 00 No access may be performed by this master.
               + LSHIFT(0x00, 10) // M5AP[1:0] | 00 No access may be performed by this master.
               + LSHIFT(0x00, 8)  // M4AP[1:0] | 00 No access may be performed by this master.
               + LSHIFT(0x03, 6)  // M3AP[1:0] | 11 Both read and write accesses may be performed by this master
               + LSHIFT(0x03, 4)  // M2AP[1:0] | 11 Both read and write accesses may be performed by this master
               + LSHIFT(0x03, 2)  // M1AP[1:0] | 11 Both read and write accesses may be performed by this master
               + LSHIFT(0x03, 0)  // M0AP[1:0] | 11 Both read and write accesses may be performed by this master
  ;

  SIM->SCGC6 |= BIT(18) ; // Включаем модуль CRC


  __set_BASEPRI(BASEPRI << 4);
  __enable_interrupt();
}

/*
** ===================================================================
**     Method      :  Cpu_SetBASEPRI (component MK60FN1M0LQ15)
**
**     Description :
**         This method sets the BASEPRI core register.
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
/*lint -save  -e586 -e950 Disable MISRA rule (2.1,1.1) checking. */
#ifdef _lint
  #define Cpu_SetBASEPRI(Level)  /* empty */
#else
void Cpu_SetBASEPRI(uint32_t Level)
{
  asm("msr basepri, %[input]"::[input] "r" (Level):);
}
#endif
/*lint -restore Enable MISRA rule (2.1,1.1) checking. */





#ifdef __cplusplus
}  /* extern "C" */
#endif

