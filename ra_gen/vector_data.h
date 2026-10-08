/* generated vector header file - do not edit */
        #ifndef VECTOR_DATA_H
        #define VECTOR_DATA_H
        #ifdef __cplusplus
        extern "C" {
        #endif
                /* Number of interrupts allocated */
        #ifndef VECTOR_DATA_IRQ_COUNT
        #define VECTOR_DATA_IRQ_COUNT    (17)
        #endif
        /* ISR prototypes */
        void gpt_counter_overflow_isr(void);
        void glcdc_line_detect_isr(void);
        void glcdc_underflow_1_isr(void);
        void mipi_dsi_seq0_isr(void);
        void mipi_dsi_seq1_isr(void);
        void mipi_dsi_vin1_isr(void);
        void mipi_dsi_rcv_isr(void);
        void mipi_dsi_ferr_isr(void);
        void mipi_dsi_ppi_isr(void);
        void sci_b_uart_rxi_isr(void);
        void sci_b_uart_txi_isr(void);
        void sci_b_uart_tei_isr(void);
        void sci_b_uart_eri_isr(void);
        void spi_b_rxi_isr(void);
        void spi_b_txi_isr(void);
        void spi_b_tei_isr(void);
        void spi_b_eri_isr(void);

        /* Vector table allocations */
        #define VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW ((IRQn_Type) 0) /* GPT0 COUNTER OVERFLOW (Overflow) */
        #define GPT0_COUNTER_OVERFLOW_IRQn          ((IRQn_Type) 0) /* GPT0 COUNTER OVERFLOW (Overflow) */
        #define VECTOR_NUMBER_GLCDC_LINE_DETECT ((IRQn_Type) 1) /* GLCDC LINE DETECT (Specified line) */
        #define GLCDC_LINE_DETECT_IRQn          ((IRQn_Type) 1) /* GLCDC LINE DETECT (Specified line) */
        #define VECTOR_NUMBER_GLCDC_UNDERFLOW_1 ((IRQn_Type) 2) /* GLCDC UNDERFLOW 1 (Graphic 1 underflow) */
        #define GLCDC_UNDERFLOW_1_IRQn          ((IRQn_Type) 2) /* GLCDC UNDERFLOW 1 (Graphic 1 underflow) */
        #define VECTOR_NUMBER_MIPIDSI_SEQ0 ((IRQn_Type) 3) /* MIPIDSI SEQ0 (Sequence operation channel 0 interrupt) */
        #define MIPIDSI_SEQ0_IRQn          ((IRQn_Type) 3) /* MIPIDSI SEQ0 (Sequence operation channel 0 interrupt) */
        #define VECTOR_NUMBER_MIPIDSI_SEQ1 ((IRQn_Type) 4) /* MIPIDSI SEQ1 (Sequence operation channel 1 interrupt) */
        #define MIPIDSI_SEQ1_IRQn          ((IRQn_Type) 4) /* MIPIDSI SEQ1 (Sequence operation channel 1 interrupt) */
        #define VECTOR_NUMBER_MIPIDSI_VIN1 ((IRQn_Type) 5) /* MIPIDSI VIN1 (Video-Input operation channel1 interrupt) */
        #define MIPIDSI_VIN1_IRQn          ((IRQn_Type) 5) /* MIPIDSI VIN1 (Video-Input operation channel1 interrupt) */
        #define VECTOR_NUMBER_MIPIDSI_RCV ((IRQn_Type) 6) /* MIPIDSI RCV (DSI packet receive interrupt) */
        #define MIPIDSI_RCV_IRQn          ((IRQn_Type) 6) /* MIPIDSI RCV (DSI packet receive interrupt) */
        #define VECTOR_NUMBER_MIPIDSI_FERR ((IRQn_Type) 7) /* MIPIDSI FERR (DSI fatal error interrupt) */
        #define MIPIDSI_FERR_IRQn          ((IRQn_Type) 7) /* MIPIDSI FERR (DSI fatal error interrupt) */
        #define VECTOR_NUMBER_MIPIDSI_PPI ((IRQn_Type) 8) /* MIPIDSI PPI (DSI D-PHY PPI interrupt) */
        #define MIPIDSI_PPI_IRQn          ((IRQn_Type) 8) /* MIPIDSI PPI (DSI D-PHY PPI interrupt) */
        #define VECTOR_NUMBER_SCI3_RXI ((IRQn_Type) 9) /* SCI3 RXI (Receive data full) */
        #define SCI3_RXI_IRQn          ((IRQn_Type) 9) /* SCI3 RXI (Receive data full) */
        #define VECTOR_NUMBER_SCI3_TXI ((IRQn_Type) 10) /* SCI3 TXI (Transmit data empty) */
        #define SCI3_TXI_IRQn          ((IRQn_Type) 10) /* SCI3 TXI (Transmit data empty) */
        #define VECTOR_NUMBER_SCI3_TEI ((IRQn_Type) 11) /* SCI3 TEI (Transmit end) */
        #define SCI3_TEI_IRQn          ((IRQn_Type) 11) /* SCI3 TEI (Transmit end) */
        #define VECTOR_NUMBER_SCI3_ERI ((IRQn_Type) 12) /* SCI3 ERI (Receive error) */
        #define SCI3_ERI_IRQn          ((IRQn_Type) 12) /* SCI3 ERI (Receive error) */
        #define VECTOR_NUMBER_SPI1_RXI ((IRQn_Type) 13) /* SPI1 RXI (Receive buffer full) */
        #define SPI1_RXI_IRQn          ((IRQn_Type) 13) /* SPI1 RXI (Receive buffer full) */
        #define VECTOR_NUMBER_SPI1_TXI ((IRQn_Type) 14) /* SPI1 TXI (Transmit buffer empty) */
        #define SPI1_TXI_IRQn          ((IRQn_Type) 14) /* SPI1 TXI (Transmit buffer empty) */
        #define VECTOR_NUMBER_SPI1_TEI ((IRQn_Type) 15) /* SPI1 TEI (Transmission complete event) */
        #define SPI1_TEI_IRQn          ((IRQn_Type) 15) /* SPI1 TEI (Transmission complete event) */
        #define VECTOR_NUMBER_SPI1_ERI ((IRQn_Type) 16) /* SPI1 ERI (Error) */
        #define SPI1_ERI_IRQn          ((IRQn_Type) 16) /* SPI1 ERI (Error) */
        /* The number of entries required for the ICU vector table. */
        #define BSP_ICU_VECTOR_NUM_ENTRIES (17)

        #ifdef __cplusplus
        }
        #endif
        #endif /* VECTOR_DATA_H */