#define _SAMCOP_OWM_Command                0x0000    /* R/W 4 bits command register */
#define _SAMCOP_OWM_Data                   0x0004    /* R/W 8 bits, transmit / receive buffer */
#define _SAMCOP_OWM_Interrupt              0x0008    /* R/W Command register */
#define _SAMCOP_OWM_InterruptEnable        0x000c    /* R/W Command register */
#define _SAMCOP_OWM_ClockDivisor           0x0010    /* R/W 5 bits of divisor and pre-scale */

#define SAMCOP_OWM_REGISTER(_b,s,x,y)					\
     (*((volatile s *) (_b + (_SAMCOP_ ## x ## _ ## y))))

#define SAMCOP_OWM_Command(_b)            SAMCOP_OWM_REGISTER( _b, u8, OWM, Command )
#define SAMCOP_OWM_Data(_b)               SAMCOP_OWM_REGISTER( _b, u8, OWM, Data )
#define SAMCOP_OWM_Interrupt(_b)          SAMCOP_OWM_REGISTER( _b, u8, OWM, Interrupt )
#define SAMCOP_OWM_InterruptEnable(_b)    SAMCOP_OWM_REGISTER( _b, u8, OWM, InterruptEnable )
#define SAMCOP_OWM_ClockDivisor(_b)       SAMCOP_OWM_REGISTER( _b, u8, OWM, ClockDivisor )

#define OWM_CMD_ONE_WIRE_RESET ( 1 << 0 )    /* Set to force reset on 1-wire bus */
#define OWM_CMD_SRA            ( 1 << 1 )    /* Set to switch to Search ROM accelerator mode */
#define OWM_CMD_DQ_OUTPUT      ( 1 << 2 )    /* Write only - forces bus low */
#define OWM_CMD_DQ_INPUT       ( 1 << 3 )    /* Read only - reflects state of bus */

#define OWM_INT_PD             ( 1 << 0 )    /* Presence detect */
#define OWM_INT_PDR            ( 1 << 1 )    /* Presence detect result */
#define OWM_INT_TBE            ( 1 << 2 )    /* Transmit buffer empty */
#define OWM_INT_TSRE           ( 1 << 3 )    /* Transmit shift register empty */
#define OWM_INT_RBF            ( 1 << 4 )    /* Receive buffer full */
#define OWM_INT_RSRF           ( 1 << 5 )    /* Receive shift register full */

#define OWM_INTEN_EPD          ( 1 << 0 )    /* Enable presence detect interrupt */
#define OWM_INTEN_IAS          ( 1 << 1 )    /* INTR active state */
#define OWM_INTEN_ETBE         ( 1 << 2 )    /* Enable transmit buffer empty interrupt */
#define OWM_INTEN_ETMT         ( 1 << 3 )    /* Enable transmit shift register empty interrupt */
#define OWM_INTEN_ERBF         ( 1 << 4 )    /* Enable receive buffer full interrupt */
#define OWM_INTEN_ERSRF        ( 1 << 5 )    /* Enable receive shift register full interrupt */
#define OWM_INTEN_DQO          ( 1 << 6 )    /* Enable direct bus driving operations (Undocumented), Szabolcs Gyurko */
