#ifndef FRAME_PARSER_H_
#define FRAME_PARSER_H_

#include <stdint.h>

#define FRAME_HDR1      0xAA
#define FRAME_HDR2_VAL  0x55
#define FRAME_DATA_MAX  254

#define FRAME_CMD_MENU  0x01
#define FRAME_CMD_BACK  0x02
#define FRAME_CMD_ECHO  0x03

typedef struct {
    uint8_t cmd;
    uint8_t data[FRAME_DATA_MAX];
    uint8_t data_len;
} Frame_t;

typedef void (*FrameHandler_t)(const Frame_t *frame);

typedef struct {
    uint8_t state;
    uint8_t len;
    uint8_t body_pos;
    uint8_t xor_calc;
    Frame_t frame;
    FrameHandler_t handler;
} FrameParser_t;

void FrameParserInit(FrameParser_t *fp, FrameHandler_t handler);
void FrameParserFeed(FrameParser_t *fp, uint8_t byte);

#endif /* FRAME_PARSER_H_ */
