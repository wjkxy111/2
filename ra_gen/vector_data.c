/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = gpt_counter_overflow_isr, /* GPT0 COUNTER OVERFLOW (Overflow) */
            [1] = glcdc_line_detect_isr, /* GLCDC LINE DETECT (Specified line) */
            [2] = glcdc_underflow_1_isr, /* GLCDC UNDERFLOW 1 (Graphic 1 underflow) */
            [3] = mipi_dsi_seq0_isr, /* MIPIDSI SEQ0 (Sequence operation channel 0 interrupt) */
            [4] = mipi_dsi_seq1_isr, /* MIPIDSI SEQ1 (Sequence operation channel 1 interrupt) */
            [5] = mipi_dsi_vin1_isr, /* MIPIDSI VIN1 (Video-Input operation channel1 interrupt) */
            [6] = mipi_dsi_rcv_isr, /* MIPIDSI RCV (DSI packet receive interrupt) */
            [7] = mipi_dsi_ferr_isr, /* MIPIDSI FERR (DSI fatal error interrupt) */
            [8] = mipi_dsi_ppi_isr, /* MIPIDSI PPI (DSI D-PHY PPI interrupt) */
            [9] = sci_b_uart_rxi_isr, /* SCI3 RXI (Receive data full) */
            [10] = sci_b_uart_txi_isr, /* SCI3 TXI (Transmit data empty) */
            [11] = sci_b_uart_tei_isr, /* SCI3 TEI (Transmit end) */
            [12] = sci_b_uart_eri_isr, /* SCI3 ERI (Receive error) */
            [13] = spi_b_rxi_isr, /* SPI1 RXI (Receive buffer full) */
            [14] = spi_b_txi_isr, /* SPI1 TXI (Transmit buffer empty) */
            [15] = spi_b_tei_isr, /* SPI1 TEI (Transmission complete event) */
            [16] = spi_b_eri_isr, /* SPI1 ERI (Error) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_GPT0_COUNTER_OVERFLOW,GROUP0), /* GPT0 COUNTER OVERFLOW (Overflow) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_GLCDC_LINE_DETECT,GROUP1), /* GLCDC LINE DETECT (Specified line) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_GLCDC_UNDERFLOW_1,GROUP2), /* GLCDC UNDERFLOW 1 (Graphic 1 underflow) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_SEQ0,GROUP3), /* MIPIDSI SEQ0 (Sequence operation channel 0 interrupt) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_SEQ1,GROUP4), /* MIPIDSI SEQ1 (Sequence operation channel 1 interrupt) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_VIN1,GROUP5), /* MIPIDSI VIN1 (Video-Input operation channel1 interrupt) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_RCV,GROUP6), /* MIPIDSI RCV (DSI packet receive interrupt) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_FERR,GROUP7), /* MIPIDSI FERR (DSI fatal error interrupt) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_PPI,GROUP0), /* MIPIDSI PPI (DSI D-PHY PPI interrupt) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_SCI3_RXI,GROUP1), /* SCI3 RXI (Receive data full) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_SCI3_TXI,GROUP2), /* SCI3 TXI (Transmit data empty) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_SCI3_TEI,GROUP3), /* SCI3 TEI (Transmit end) */
            [12] = BSP_PRV_VECT_ENUM(EVENT_SCI3_ERI,GROUP4), /* SCI3 ERI (Receive error) */
            [13] = BSP_PRV_VECT_ENUM(EVENT_SPI1_RXI,GROUP5), /* SPI1 RXI (Receive buffer full) */
            [14] = BSP_PRV_VECT_ENUM(EVENT_SPI1_TXI,GROUP6), /* SPI1 TXI (Transmit buffer empty) */
            [15] = BSP_PRV_VECT_ENUM(EVENT_SPI1_TEI,GROUP7), /* SPI1 TEI (Transmission complete event) */
            [16] = BSP_PRV_VECT_ENUM(EVENT_SPI1_ERI,GROUP0), /* SPI1 ERI (Error) */
        };
        #endif
        #endif