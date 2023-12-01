#include <cstdint>

namespace witmotion_protocol
{

    struct wp_angle_message
    {
        uint8_t RollL;
        uint8_t RollH;
        uint8_t PitchL;
        uint8_t PitchH;
        uint8_t YawL;
        uint8_t YawH;
        uint8_t VL;
        uint8_t VH;
        uint8_t SUM_CRC;
    };
    
    enum header: uint8_t
    {
        MESSAGE_HEADER = 0x55,
        ANGLE = 0x53, 
    };

    enum state: uint8_t
    {
        HEADER,
        TYPE_HEADER,
        PAYLOAD,
        IDLE,
    };

};

