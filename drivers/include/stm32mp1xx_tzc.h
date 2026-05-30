
typedef struct {
    uint32_t REGION_BASE_LOW0;
    uint32_t REGION_BASE_HIGH0;
    uint32_t REGION_TOP_LOW0;
    uint32_t REGION_TOP_HIGH0;
    uint32_t TREGION_ATTRIBUTE0;
    uint32_t REGION_ID_ACCESS0;
    uint8_t  RSVD0[0x120 - 0x114 - 4];
} TzcRegionRegs_t;


typedef struct {
    uint32_t BUILD_CONFIG;
    uint32_t ACTION;
    uint32_t GATE_KEEPER;
    uint32_t SPECULATION_CTRL;
    uint32_t INT_STATUS;
    uint32_t INT_CLEAR;
    uint8_t  RSVD0[0x20 - 0x14 - 4];
    uint32_t FAIL_ADDRESS_LOW0;
    uint32_t FAIL_ADDRESS_HIGH0;
    uint32_t FAIL_CONTROL0;
    uint32_t FAIL_ID0;
    uint32_t FAIL_ADDRESS_LOW1;
    uint32_t FAIL_ADDRESS_HIGH1;
    uint32_t FAIL_CONTROL1;
    uint32_t FAIL_ID1;
    uint8_t  RSVD1[0x100 - 0x3C - 4];
    TzcRegionRegs_t REGION[9];
    uint8_t  RSVD2[0xFD0 - 0x214 - 4];
    uint32_t PID4;
    uint32_t PID5;
    uint32_t PID6;
    uint32_t PID7;
    uint32_t PID0;
    uint32_t PID1;
    uint32_t DRASC_PID2;
    uint32_t PID3;
    uint32_t CID0;
    uint32_t CID1;
    uint32_t CID2;
    uint32_t CID3;
} TzcRegs_t;


extern volatile TzcRegs_t *const TZC;