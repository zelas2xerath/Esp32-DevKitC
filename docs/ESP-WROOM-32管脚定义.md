### ESP-WROOM-32管脚定义

| 名称      | 序号 | 功能描述                                                     |
| --------- | ---- | ------------------------------------------------------------ |
| GND       | 1    | 接地                                                         |
| 3V3       | 2    | 供电                                                         |
| EN        | 3    | 使能芯片，高电平有效。                                       |
| SENSOR_VP | 4    | GPIO36, SENSOR_VP, ADC_H, ADC1_CH0, RTC_GPIO0                |
| SENSOR_VN | 5    | GPIO39, SENSOR_VN, ADC1_CH3, ADC_H, RTC_GPIO3                |
| IO34      | 6    | GPIO34, ADC1_CH6, RTC_GPIO4                                  |
| IO35      | 7    | GPIO35, ADC1_CH7, RTC_GPIO5                                  |
| IO32      | 8    | GPIO32, XTAL_32K_P（32.768 kHz 晶振输入）, ADC1_CH4, TOUCH9, RTC_GPIO9 |
| IO33      | 9    | GPIO33, XTAL_32K_N（32.768 kHz 晶振输出）, ADC1_CH5, TOUCH8, RTC_GPIO8 |
| IO25      | 10   | GPIO25, DAC_1, ADC2_CH8, RTC_GPIO6, EMAC_RXD0                |
| IO26      | 11   | GPIO26, DAC_2, ADC2_CH9, RTC_GPIO7, EMAC_RXD1                |
| IO27      | 12   | GPIO27, ADC2_CH7, TOUCH7, RTC_GPIO17, EMAC_RX_DV             |
| IO14      | 13   | GPIO14, ADC2_CH6, TOUCH6, RTC_GPIO16, MTMS, HSPI_CLK, HS2_CLK, SD_CLK, EMAC_TX2 |
| IO12      | 14   | GPIO12, ADC2_CH5, TOUCH5, RTC_GPIO15, MTDI, HSPI_Q, HS2_DATA2, SD_DATA2, EMAC_TX3 |
| GND       | 15   | 接地                                                         |
| IO13      | 16   | GPIO13, ADC2_CH4, TOUCH4, RTC_GPIO14, MTCK, HSPI_SD, HS2_DATA3, SD_DATA3, EMAC_RX_ER |
| SHD/SD2   | 17   | GPIO9, SD_DATA2, SPIHD, HS1_DATA2, U1RXD                     |
| SWP/SD3   | 18   | GPIO10, SD_DATA3, SPIWP, HS1_DATA3, U1TXD                    |
| SCS/CMD   | 19   | GPIO11, SD_CMD, SPICS0, HS1_CMD, U1RTS                       |
| SCK/CLK   | 20   | GPIO6, SD_CLK, SPICLK, HS1_CLK, U1CTS                        |
| SDO/SD0   | 21   | GPIO7, SD_DATA0, SPIQ, HS1_DATA0, U2RTS                      |
| SDI/SD1   | 22   | GPIO8, SD_DATA1, SPID, HS1_DATA1, U2CTS                      |
| IO15      | 23   | GPIO15, ADC2_CH3, TOUCH3, MTDO, HSPI_CS0, RTC_GPIO13, HS2_CMD, SD_CMD, EMAC_RXD3 |
| IO2       | 24   | GPIO2, ADC2_CH2, TOUCH2, RTC_GPIO12, HSPIWP, HS2_DATA0, SD_DATA0 |
| IO0       | 25   | GPIO0, ADC2_CH1, TOUCH1, RTC_GPIO11, CLK_OUT1, EMAC_TX_CLK   |
| IO4       | 26   | GPIO4, ADC2_CH0, TOUCH0, RTC_GPIO10, HSPIHD, HS2_DATA1, SD_DATA1, EMAC_TX_ER |
| IO16      | 27   | GPIO16, HS1_DATA4, U2RXD, EMAC_CLK_OUT                       |
| IO17      | 28   | GPIO17, HS1_DATA5, U2TXD, EMAC_CLK_OUT_180                   |
| IO5       | 29   | GPIO5, VSPI_CS0, HS1_DATA6, EMAC_RX_CLK                      |
| IO18      | 30   | GPIO18, VSPI_CLK, HS1_DATA7                                  |
| IO19      | 31   | GPIO19, VSPI_Q, U0CTS, EMAC_TXD0                             |
| NC        | 32   | 无连接（No Connection）                                      |
| IO21 | 33   | GPIO21, VSPIHD, EMAC_TX_EN        |
| RXD0 | 34   | GPIO3, U0RXD, CLK_OUT2            |
| TXD0 | 35   | GPIO1, U0TXD, CLK_OUT3, EMAC_RXD2 |
| IO22 | 36   | GPIO22, VSPIWP, UORTS, EMAC_TXD1  |
| IO23 | 37   | GPIO23, VSPID, HS1_STROBE         |
| GND  | 38   | 接地                              |

