/*
 * si5351.h - SI5351 Clock Generator driver for RPi
 * 01-08-2017 E. Brombaugh
 */

#ifndef __si5351__
#define __si5351__

#include "main.h"

/* register definitions */
#define SI5351_0_Device_Status 0
#define SI5351_1_Interrupt_Status_Sticky 1
#define SI5351_2_Interrupt_Status_Mask 2
#define SI5351_3_Output_Enable_Control 3
#define SI5351_9_OEB_Pin_Enable_Control_Mask 9
#define SI5351_15_PLL_Input_Source 15
#define SI5351_16_CLK0_Control 16
#define SI5351_17_CLK1_Control 17
#define SI5351_18_CLK2_Control 18
#define SI5351_19_CLK3_Control 19
#define SI5351_20_CLK4_Control 20
#define SI5351_21_CLK5_Control 21
#define SI5351_22_CLK6_Control 22
#define SI5351_23_CLK7_Control 23
#define SI5351_24_CLK3_0_Disable_State 24
#define SI5351_25_CLK7_4_Disable_State 25
#define SI5351_26_Multisynth_NA_Parameters_0 26
#define SI5351_27_Multisynth_NA_Parameters_1 27
#define SI5351_28_Multisynth_NA_Parameters_2 28
#define SI5351_29_Multisynth_NA_Parameters_3 29
#define SI5351_30_Multisynth_NA_Parameters_4 30
#define SI5351_31_Multisynth_NA_Parameters_5 31
#define SI5351_32_Multisynth_NA_Parameters_6 32
#define SI5351_33_Multisynth_NA_Parameters_7 33
#define SI5351_34_Multisynth_NB_Parameters_0 34
#define SI5351_35_Multisynth_NB_Parameters_1 35
#define SI5351_36_Multisynth_NB_Parameters_2 36
#define SI5351_37_Multisynth_NB_Parameters_3 37
#define SI5351_38_Multisynth_NB_Parameters_4 38
#define SI5351_39_Multisynth_NB_Parameters_5 39
#define SI5351_40_Multisynth_NB_Parameters_6 40
#define SI5351_41_Multisynth_NB_Parameters_7 41
#define SI5351_42_Multisynth0_Parameters_0 42
#define SI5351_43_Multisynth0_Parameters_1 43
#define SI5351_44_Multisynth0_Parameters_2 44
#define SI5351_45_Multisynth0_Parameters_3 45
#define SI5351_46_Multisynth0_Parameters_4 46
#define SI5351_47_Multisynth0_Parameters_5 47
#define SI5351_48_Multisynth0_Parameters_6 48
#define SI5351_49_Multisynth0_Parameters_7 49
#define SI5351_50_Multisynth1_Parameters_0 50
#define SI5351_51_Multisynth1_Parameters_1 51
#define SI5351_52_Multisynth1_Parameters_2 52
#define SI5351_53_Multisynth1_Parameters_3 53
#define SI5351_54_Multisynth1_Parameters_4 54
#define SI5351_55_Multisynth1_Parameters_5 55
#define SI5351_56_Multisynth1_Parameters_6 56
#define SI5351_57_Multisynth1_Parameters_7 57
#define SI5351_58_Multisynth2_Parameters_0 58
#define SI5351_59_Multisynth2_Parameters_1 59
#define SI5351_60_Multisynth2_Parameters_2 60
#define SI5351_61_Multisynth2_Parameters_3 61
#define SI5351_62_Multisynth2_Parameters_4 62
#define SI5351_63_Multisynth2_Parameters_5 63
#define SI5351_64_Multisynth2_Parameters_6 64
#define SI5351_65_Multisynth2_Parameters_7 65
#define SI5351_66_Multisynth3_Parameters_0 66
#define SI5351_67_Multisynth3_Parameters_1 67
#define SI5351_68_Multisynth3_Parameters_2 68
#define SI5351_69_Multisynth3_Parameters_3 69
#define SI5351_70_Multisynth3_Parameters_4 70
#define SI5351_71_Multisynth3_Parameters_5 71
#define SI5351_72_Multisynth3_Parameters_6 72
#define SI5351_73_Multisynth3_Parameters_7 73
#define SI5351_74_Multisynth4_Parameters_0 74
#define SI5351_75_Multisynth4_Parameters_1 75
#define SI5351_76_Multisynth4_Parameters_2 76
#define SI5351_77_Multisynth4_Parameters_3 77
#define SI5351_78_Multisynth4_Parameters_4 78
#define SI5351_79_Multisynth4_Parameters_5 79
#define SI5351_80_Multisynth4_Parameters_6 80
#define SI5351_81_Multisynth4_Parameters_7 81
#define SI5351_82_Multisynth5_Parameters_0 82
#define SI5351_83_Multisynth5_Parameters_1 83
#define SI5351_84_Multisynth5_Parameters_2 84
#define SI5351_85_Multisynth5_Parameters_3 85
#define SI5351_86_Multisynth5_Parameters_4 86
#define SI5351_87_Multisynth5_Parameters_5 87
#define SI5351_88_Multisynth5_Parameters_6 88
#define SI5351_89_Multisynth5_Parameters_7 89
#define SI5351_90_Multisynth6_Parameters_0 90
#define SI5351_91_Multisynth7_Parameters_0 91
#define SI5351_92_Clock_6_and_7_Output_Divider 92
#define SI5351_149_Spread_Spectrum_Parameters_0 149
#define SI5351_150_Spread_Spectrum_Parameters_1 150
#define SI5351_151_Spread_Spectrum_Parameters_2 151
#define SI5351_152_Spread_Spectrum_Parameters_3 152
#define SI5351_153_Spread_Spectrum_Parameters_4 153
#define SI5351_154_Spread_Spectrum_Parameters_5 154
#define SI5351_155_Spread_Spectrum_Parameters_6 155
#define SI5351_156_Spread_Spectrum_Parameters_7 156
#define SI5351_157_Spread_Spectrum_Parameters_9 157
#define SI5351_158_Spread_Spectrum_Parameters_A 158
#define SI5351_159_Spread_Spectrum_Parameters_B 159
#define SI5351_160_Spread_Spectrum_Parameters_C 160
#define SI5351_161_Spread_Spectrum_Parameters_D 161
#define SI5351_162_VCXO_Parameter_0 162
#define SI5351_163_VCXO_Parameter_1 163
#define SI5351_164_VCXO_Parameter_2 164
#define SI5351_165_CLK0_Initial_Phase_Offset 165
#define SI5351_166_CLK1_Initial_Phase_Offset 166
#define SI5351_167_CLK2_Initial_Phase_Offset 167
#define SI5351_168_CLK3_Initial_Phase_Offset 168
#define SI5351_169_CLK4_Initial_Phase_Offset 169
#define SI5351_170_CLK5_Initial_Phase_Offset 170
#define SI5351_177_PLL_Reset 177
#define SI5351_183_Crystal_Internal_Load_Capacitance 183
#define SI5351_187_Fanout_Enable 187


uint8_t si5351_init(uint8_t verbose);
uint8_t si5351_set_output_chl(uint8_t chl, uint32_t freq);

#endif
