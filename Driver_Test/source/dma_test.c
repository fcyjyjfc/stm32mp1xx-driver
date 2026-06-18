#include "test_common.h"


void DmaTest(void)
{
    PRINT("=== DMA TX + RX ===\r\n");
    PRINT("TX: DMA ring buffer (global)\r\n");
    PRINT("RX: DMA circular + frame parser (global)\r\n");
    PRINT("Frame: AA 55 LEN CMD [DATA] XOR\r\n");
    PRINT("CMD 01: Menu select (DATA=key)\r\n");
    PRINT("CMD 02: Back/exit\r\n");
    PRINT("CMD 03: Echo (DATA echoed as hex)\r\n");
    PRINT("Send CMD 03 to test echo, CMD 02 to exit.\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        Frame_t fr;
        if (FramePoll(&fr) && fr.cmd == FRAME_CMD_BACK)
            break;
    }
}
