#include <ti/drv/i2c/I2C.h>
#include <ti/csl/hw_types.h>

#define I2C_CODEC_ADDR           (0x1aU)
#define I2C_CODEC_INSTANCE       (3U)
#define I2C_TRANSACTION_TIMEOUT  (500U)
#define LICZBA_REJ_WM8960    (26U)

static void AppDelay(uint32_t delayVal);
static void Pins_config();
static uint32_t Read_mcasp1_rcv();
static void Write_mcasp1_xmt(uint32_t probka);
static void Restart_mcasp1_if_error();
static void Config_and_start_mcasp1();
static void Config_i2c_and_codec();

#define CTRL_CORE_DSP1_IRQ_94_95 0x4A0029C4

#define rMCASP1CLKCTRL 0x4A005550

#define rEVTCTLR 0x4846007C
#define rREVTCTL 0x4846008C
#define rRXSTAT 0x48460080
#define rRXBUF11 0x484602AC

#define rEVTCTLX 0x484600BC
#define rXEVTCTL 0x484600CC
#define rTXSTAT 0x484600C0
#define rTXBUF10 0x48460228

#define rGBLCTL 0x48460044
#define rPWRIDLESYSCONFIG 0x48460004
#define rPDIR 0x48460014
#define rACLKXCTL 0x484600B0
#define rLBCTL 0x4846004C
#define rPFUNC 0x48460010

#define rRXFMT 0x48460068
#define rRXMASK 0x48460064
#define rRXTDM 0x48460078
#define rXRSRCTL11 0x484601AC

#define rTXFMT 0x484600A8
#define rTXMASK 0x484600A4
#define rTXFMCTL 0x484600AC
#define rTXTDM 0x484600B8
#define rXRSRCTL10 0x484601A8
#define rTXDITCTL 0x48460050

#define P9_19a_ballR6 0x4A003440
#define P9_19b_ballF4 0x4A00357C
#define P9_20a_ballT9 0x4A003444
#define P9_20b_ballD2 0x4A003578
#define P9_28_ballA12  0x4A0036E0
#define P9_29a_ballA11 0x4A0036D8
#define P9_29b_ballD14 0x4A0036A8
#define P9_30_ballB13  0x4A0036DC
#define P9_31a_ballB12 0x4A0036D4
#define P9_31b_ballC14 0x4A0036A4

static void Pins_config()
{
    HW_WR_REG32(P9_19a_ballR6,0x00060007);
    HW_WR_REG32(P9_19b_ballF4,0x0006000E);
    HW_WR_REG32(P9_20a_ballT9,0x00060007);
    HW_WR_REG32(P9_20b_ballD2,0x0006000E);
    HW_WR_REG32(P9_28_ballA12,0x00050000);
    HW_WR_REG32(P9_29a_ballA11,0x0005000E);
    HW_WR_REG32(P9_29b_ballD14,0x00050000);
    HW_WR_REG32(P9_30_ballB13,0x00010000);
    HW_WR_REG32(P9_31a_ballB12,0x0005000E);
    HW_WR_REG32(P9_31b_ballC14,0x00050000);
}

void Turn_on_mcasp1()
{
    HW_WR_REG32(rMCASP1CLKCTRL, 0x02);
    while ((HW_RD_REG32(rMCASP1CLKCTRL) & 0x00030003) != 0x02);
}

void Init_mcasp1_rcv()
{
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) & 0xFFFFFFE0);
    while (HW_RD_REG32(rGBLCTL) & 0x0000001F);
    HW_WR_REG32(rPWRIDLESYSCONFIG, (HW_RD_REG32(rPWRIDLESYSCONFIG) | 0x00000001) & 0xFFFFFFFD);
    HW_WR_REG32(rRXFMT, HW_RD_REG32(rRXFMT) | 0x000000F0);
    HW_WR_REG32(rRXMASK, 0xFFFFFFFF);
    HW_WR_REG32(rRXFMT, HW_RD_REG32(rRXFMT) & 0xFFFF9FFF);
    HW_WR_REG32(rRXFMT, HW_RD_REG32(rRXFMT) & 0xFFFFE0FF);
    HW_WR_REG32(rRXFMT, HW_RD_REG32(rRXFMT) & 0xFFFFFFF8);
    HW_WR_REG32(rRXFMT, HW_RD_REG32(rRXFMT) | 0x00008000);
    HW_WR_REG32(rRXFMT, HW_RD_REG32(rRXFMT) & 0xFFFCFFFF);
    HW_WR_REG32(rRXFMT, HW_RD_REG32(rRXFMT) | 0x00000008);
    HW_WR_REG32(rACLKXCTL, HW_RD_REG32(rACLKXCTL) & 0xFFFFFFBF);
    HW_WR_REG32(rRXTDM, 1);
    HW_WR_REG32(rXRSRCTL11, (HW_RD_REG32(rXRSRCTL11) | 0x00000002) & 0xFFFFFFFE);
    HW_WR_REG32(rPFUNC, 0);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0x7FFFFFFF);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0xDFFFFFFF);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0xFFFFF7FF);
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000002);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000002));
}

void Init_mcasp1_xmt()
{
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) & 0xFFFFE0FF);
    while (HW_RD_REG32(rGBLCTL) & 0x00001F00);
    HW_WR_REG32(rPWRIDLESYSCONFIG, (HW_RD_REG32(rPWRIDLESYSCONFIG) | 0x00000001) & 0xFFFFFFFD);
    HW_WR_REG32(rTXFMT, HW_RD_REG32(rTXFMT) | 0x000000F0);
    HW_WR_REG32(rTXMASK, 0xFFFFFFFF);
    HW_WR_REG32(rTXFMT, HW_RD_REG32(rTXFMT) & 0xFFFF9FFF);
    HW_WR_REG32(rTXFMT, HW_RD_REG32(rTXFMT) & 0xFFFFE0FF);
    HW_WR_REG32(rTXFMT, HW_RD_REG32(rTXFMT) & 0xFFFFFFF8);
    HW_WR_REG32(rTXFMT, HW_RD_REG32(rTXFMT) | 0x00008000);
    HW_WR_REG32(rTXFMT, HW_RD_REG32(rTXFMT) & 0xFFFCFFFF);
    HW_WR_REG32(rTXFMT, HW_RD_REG32(rTXFMT) | 0x00000008);
    HW_WR_REG32(rTXFMCTL, (HW_RD_REG32(rTXFMCTL) | 0x00000000) & 0xFFFF017F);
    HW_WR_REG32(rTXFMCTL, HW_RD_REG32(rTXFMCTL) & 0xFFFFFFEF);
    HW_WR_REG32(rTXFMCTL, HW_RD_REG32(rTXFMCTL) & 0xFFFFFFFE);
    HW_WR_REG32(rTXFMCTL, HW_RD_REG32(rTXFMCTL) & 0xFFFFFFFD);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0xEFFFFFFF);
    HW_WR_REG32(rACLKXCTL, HW_RD_REG32(rACLKXCTL) & 0xFFFFFFDF);
    HW_WR_REG32(rACLKXCTL, HW_RD_REG32(rACLKXCTL) | 0x00000080);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0xFBFFFFFF);
    HW_WR_REG32(rTXTDM, 1);
    HW_WR_REG32(rXRSRCTL10, (HW_RD_REG32(rXRSRCTL10) | 0x00000001) & 0xFFFFFFFD);
    HW_WR_REG32(rTXDITCTL, HW_RD_REG32(rTXDITCTL) & 0xFFFFFFFE);
    HW_WR_REG32(rPFUNC, 0);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0xEFFFFFFF);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0xF7FFFFFF);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) & 0xFBFFFFFF);
    HW_WR_REG32(rPDIR, HW_RD_REG32(rPDIR) | 0x00000400);
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000200);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000200));
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000100);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000100));
}

void Init_interrupt_mcasp1_rcv()
{
    HW_WR_REG32(rREVTCTL, HW_RD_REG32(rREVTCTL) | 0x00000001);
    HW_WR_REG32(rEVTCTLR, HW_RD_REG32(rEVTCTLR) | 0x00000020);
    HW_WR_REG32(rEVTCTLR, HW_RD_REG32(rEVTCTLR) & 0xFFFFFFF7);
}

void Init_interrupt_mcasp1_xmt()
{
    HW_WR_REG32(rXEVTCTL, HW_RD_REG32(rXEVTCTL) | 0x00000001);
    HW_WR_REG32(rEVTCTLX, HW_RD_REG32(rEVTCTLX) | 0x00000020);
    HW_WR_REG32(rEVTCTLX, HW_RD_REG32(rEVTCTLX) & 0xFFFFFFF7);
}

void Startup_mcasp1_rcv()
{
    HW_WR_REG32(rRXSTAT, HW_RD_REG32(rRXSTAT) | 0x000000FF);
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000004);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000004));
    int rr = HW_RD_REG32(rRXBUF11);
    while (HW_RD_REG32(rRXSTAT) & 0x00000020);
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000008);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000008));
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000010);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000010));
}

void Startup_mcasp1_xmt()
{
    HW_WR_REG32(rTXSTAT, HW_RD_REG32(rTXSTAT) | 0x000000FF);
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000400);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000400));
    HW_WR_REG32(rTXBUF10, HW_RD_REG32(rTXBUF10));
    while (HW_RD_REG32(rTXSTAT) & 0x00000020);
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00000800);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00000800));
    HW_WR_REG32(rGBLCTL, HW_RD_REG32(rGBLCTL) | 0x00001000);
    while (!(HW_RD_REG32(rGBLCTL) & 0x00001000));
}

static uint32_t Read_mcasp1_rcv()
{
    return(HW_RD_REG32(rRXBUF11));
}

static void Write_mcasp1_xmt(uint32_t probka)
{
    HW_WR_REG32(rTXBUF10, probka);
}

static void Restart_mcasp1_if_error()
{
    if (HW_RD_REG32(rTXSTAT) & 0x1) { Init_mcasp1_xmt(); Startup_mcasp1_xmt(); }
    if (HW_RD_REG32(rRXSTAT) & 0x1) { Init_mcasp1_rcv(); Startup_mcasp1_rcv(); } // do not set breakpoints here
}

static void Config_and_start_mcasp1()
{
    Turn_on_mcasp1();
    AppDelay(40);
    HW_WR_REG32(CTRL_CORE_DSP1_IRQ_94_95, 0x00670068);
    AppDelay(40);
    Init_interrupt_mcasp1_xmt();
    Init_interrupt_mcasp1_rcv();
    Init_mcasp1_xmt();
    Init_mcasp1_rcv();
    Startup_mcasp1_xmt();
    Startup_mcasp1_rcv();
}

static void Config_i2c_and_codec()
{
    I2C_Params i2cParams;
    I2C_Handle handle = NULL;
    I2C_Transaction i2cTransaction;
    uint8_t txBuf[2*LICZBA_REJ_WM8960] =
    {0x1e, 0x00, 0x45, 0x50, 0x4b, 0x50, 0x05, 0x79, 0x07, 0x79, 0x40, 0x00, 0x42, 0x00,
     0x56, 0x0a, 0x58, 0x0a, 0x0e, 0x53, 0x08, 0x48, 0x34, 0x00, 0x6A, 0x24, 0x6C, 0xdd,
     0x6E, 0x2F, 0x68, 0x36, 0x34, 0x01, 0x08, 0x49, 0x11, 0xc6, 0x32, 0xc0, 0x5e, 0x0c,
     0x0a, 0x00, 0x60, 0x02, 0x12, 0x40, 0x32, 0xfc, 0x35, 0xe1};
    uint8_t rxBuf[2] = {0x00, 0x00};
    uint8_t k;
    int16_t status;
    I2C_init();
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate=I2C_100kHz;
    i2cParams.transferMode=I2C_MODE_BLOCKING;
    handle = I2C_open(I2C_CODEC_INSTANCE, &i2cParams);
    I2C_transactionInit(&i2cTransaction);
    i2cTransaction.targetAddress = I2C_CODEC_ADDR;
    i2cTransaction.writeBuf = (uint8_t *)&txBuf[0];
    i2cTransaction.writeCount = 2;
    i2cTransaction.readBuf = (uint8_t *)&rxBuf[0];
    i2cTransaction.readCount = 0;
    i2cTransaction.timeout   = I2C_TRANSACTION_TIMEOUT;
    status = I2C_transfer(handle, &i2cTransaction);
    AppDelay(80);
    for (k=2; k < 2*LICZBA_REJ_WM8960 /* 2*9 */; k=k+2)
    {
        i2cTransaction.targetAddress = I2C_CODEC_ADDR;
        i2cTransaction.timeout   = I2C_TRANSACTION_TIMEOUT;
        i2cTransaction.writeBuf = (uint8_t *)&txBuf[k];
        status = I2C_transfer(handle, &i2cTransaction);
        AppDelay(40);
    }
    I2C_close(handle);
    if (status==1)
       {AppDelay(status);}
    AppDelay(40);
}
//int32_t stat1;

static void AppDelay(uint32_t delayVal)       // ok.(!) 2,5 mikrosekund
{
    uint32_t cnt = 0;
    while(cnt < delayVal*1000)
    {
        cnt++;
    }
}
