#ifndef CARTA_BACKEND__EVENTHEADER_H_
#define CARTA_BACKEND__EVENTHEADER_H_

namespace CARTA {
const uint16_t ICD_VERSION = 2;

enum EventType : uint16_t {
    REGISTER_VIEWER = 1,
    FILE_LIST_REQUEST = 2,
    FILE_INFO_REQUEST = 3,
    OPEN_FILE = 4,
    SET_IMAGE_VIEW = 5,
    SET_IMAGE_CHANNELS = 6,
    SET_CURSOR = 7,
    SET_SPATIAL_REQUIREMENTS = 8,
    SET_HISTOGRAM_REQUIREMENTS = 9,
    SET_STATS_REQUIREMENTS = 10,
    SET_REGION = 11,
    REMOVE_REGION = 12,
    CLOSE_FILE = 13,
    SET_SPECTRAL_REQUIREMENTS = 14,
    START_ANIMATION = 15,
    START_ANIMATION_ACK = 16,
    STOP_ANIMATION = 17,
    REGISTER_VIEWER_ACK = 18,
    FILE_LIST_RESPONSE = 19,
    FILE_INFO_RESPONSE = 20,
    OPEN_FILE_ACK = 21,
    SET_REGION_ACK = 22,
    REGION_HISTOGRAM_DATA = 23,
    RASTER_IMAGE_DATA = 24,
    SPATIAL_PROFILE_DATA = 25,
    SPECTRAL_PROFILE_DATA = 26,
    REGION_STATS_DATA = 27,
    ERROR_DATA = 28
};

struct EventHeader {
    CARTA::EventType _type;
    uint16_t _icd_vers;
    uint32_t _req_id;
};
} // namespace CARTA

#endif // CARTA_BACKEND__EVENTHEADER_H_
