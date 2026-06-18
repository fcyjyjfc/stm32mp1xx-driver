#include "frame_parser.h"

enum {
    ST_IDLE,
    ST_HDR2,
    ST_LEN,
    ST_BODY,
    ST_XOR
};


void FrameParserInit(FrameParser_t *fp, FrameHandler_t handler)
{
    fp->state    = ST_IDLE;
    fp->len      = 0;
    fp->body_pos = 0;
    fp->xor_calc = 0;
    fp->handler  = handler;
}


void FrameParserFeed(FrameParser_t *fp, uint8_t byte)
{
    switch (fp->state)
    {
    case ST_IDLE:
        if (byte == FRAME_HDR1)
            fp->state = ST_HDR2;
        break;

    case ST_HDR2:
        if (byte == FRAME_HDR2_VAL)
            fp->state = ST_LEN;
        else if (byte != FRAME_HDR1)
            fp->state = ST_IDLE;
        break;

    case ST_LEN:
        if (byte == 0)
        {
            fp->state = ST_IDLE;
            break;
        }
        fp->len      = byte;
        fp->xor_calc = byte;
        fp->body_pos = 0;
        fp->state    = ST_BODY;
        break;

    case ST_BODY:
        fp->xor_calc ^= byte;
        if (fp->body_pos == 0)
            fp->frame.cmd = byte;
        else
            fp->frame.data[fp->body_pos - 1] = byte;
        fp->body_pos++;
        if (fp->body_pos >= fp->len)
        {
            fp->frame.data_len = fp->len - 1;
            fp->state = ST_XOR;
        }
        break;

    case ST_XOR:
        if (byte == fp->xor_calc && fp->handler)
            fp->handler(&fp->frame);
        fp->state = ST_IDLE;
        break;

    default:
        fp->state = ST_IDLE;
        break;
    }
}
