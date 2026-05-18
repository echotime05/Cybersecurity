#pragma once

#include <cstdint>
#include <string_view>

namespace cyber
{
constexpr std::uint32_t kDefaultReserved = 0;
constexpr std::uint8_t kPacketHeaderSize = 11;

enum class EntityId : std::uint8_t
{
    client1 = 0x01,
    client2 = 0x02,
    client3 = 0x03,
    client4 = 0x04,
    as = 0x11,
    tgs = 0x12,
    v = 0x13,
    unknown = 0xFF
};

enum class MsgType : std::uint8_t
{
    as_req = 1,
    as_rep = 2,
    tgs_req = 3,
    tgs_rep = 4,
    v_auth_req = 5,
    v_auth_rep = 6,
    cert_c2v = 7,
    cert_v2c = 8,
    error = 101,
    app = 102
};

enum class ErrorCode : std::uint8_t
{
    password_wrong = 1,
    tgt_expired = 2,
    ticket_v_expired = 3,
    tgs_id_mismatch = 4,
    v_id_mismatch = 5,
    replay_detected = 6,
    unsupported_msg_type = 7
};

enum class AppCode : std::uint8_t
{
    key_down = 0x01,
    key_up = 0x02,
    aim_event = 0x03,
    fire_event = 0x04,
    game_join_req = 0x05,
    game_start = 0x06,
    game_state = 0x07,
    app_ack = 0x08,
    game_move = 0x09,
    game_target = 0x0A,
    game_shoot = 0x0B
};

inline bool is_known(EntityId id)
{
    switch (id)
    {
    case EntityId::client1:
    case EntityId::client2:
    case EntityId::client3:
    case EntityId::client4:
    case EntityId::as:
    case EntityId::tgs:
    case EntityId::v:
        return true;
    default:
        return false;
    }
}

inline bool is_client(EntityId id)
{
    switch (id)
    {
    case EntityId::client1:
    case EntityId::client2:
    case EntityId::client3:
    case EntityId::client4:
        return true;
    default:
        return false;
    }
}

inline bool is_server(EntityId id)
{
    switch (id)
    {
    case EntityId::as:
    case EntityId::tgs:
    case EntityId::v:
        return true;
    default:
        return false;
    }
}

inline bool is_known(MsgType type)
{
    switch (type)
    {
    case MsgType::as_req:
    case MsgType::as_rep:
    case MsgType::tgs_req:
    case MsgType::tgs_rep:
    case MsgType::v_auth_req:
    case MsgType::v_auth_rep:
    case MsgType::cert_c2v:
    case MsgType::cert_v2c:
    case MsgType::error:
    case MsgType::app:
        return true;
    default:
        return false;
    }
}

inline bool is_known(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::password_wrong:
    case ErrorCode::tgt_expired:
    case ErrorCode::ticket_v_expired:
    case ErrorCode::tgs_id_mismatch:
    case ErrorCode::v_id_mismatch:
    case ErrorCode::replay_detected:
    case ErrorCode::unsupported_msg_type:
        return true;
    default:
        return false;
    }
}

inline bool is_known(AppCode code)
{
    switch (code)
    {
    case AppCode::key_down:
    case AppCode::key_up:
    case AppCode::aim_event:
    case AppCode::fire_event:
    case AppCode::game_join_req:
    case AppCode::game_start:
    case AppCode::game_state:
    case AppCode::app_ack:
    case AppCode::game_move:
    case AppCode::game_target:
    case AppCode::game_shoot:
        return true;
    default:
        return false;
    }
}

inline std::string_view to_string(EntityId id)
{
    switch (id)
    {
    case EntityId::client1:
        return "Client1";
    case EntityId::client2:
        return "Client2";
    case EntityId::client3:
        return "Client3";
    case EntityId::client4:
        return "Client4";
    case EntityId::as:
        return "AS";
    case EntityId::tgs:
        return "TGS";
    case EntityId::v:
        return "V";
    default:
        return "Unknown";
    }
}

inline std::string_view to_string(MsgType type)
{
    switch (type)
    {
    case MsgType::as_req:
        return "MSG_AS_REQ";
    case MsgType::as_rep:
        return "MSG_AS_REP";
    case MsgType::tgs_req:
        return "MSG_TGS_REQ";
    case MsgType::tgs_rep:
        return "MSG_TGS_REP";
    case MsgType::v_auth_req:
        return "MSG_V_AUTH_REQ";
    case MsgType::v_auth_rep:
        return "MSG_V_AUTH_REP";
    case MsgType::cert_c2v:
        return "MSG_CERT_C2V";
    case MsgType::cert_v2c:
        return "MSG_CERT_V2C";
    case MsgType::error:
        return "MSG_ERROR";
    case MsgType::app:
        return "MSG_APP";
    default:
        return "UNKNOWN_MSG_TYPE";
    }
}

inline std::string_view to_string(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::password_wrong:
        return "ERR_PASSWORD_WRONG";
    case ErrorCode::tgt_expired:
        return "ERR_TGT_EXPIRED";
    case ErrorCode::ticket_v_expired:
        return "ERR_TICKET_V_EXPIRED";
    case ErrorCode::tgs_id_mismatch:
        return "ERR_TGS_ID_MISMATCH";
    case ErrorCode::v_id_mismatch:
        return "ERR_V_ID_MISMATCH";
    case ErrorCode::replay_detected:
        return "ERR_REPLAY_DETECTED";
    case ErrorCode::unsupported_msg_type:
        return "ERR_UNSUPPORTED_MSG_TYPE";
    default:
        return "UNKNOWN_ERROR";
    }
}

inline std::string_view to_string(AppCode code)
{
    switch (code)
    {
    case AppCode::key_down:
        return "KEY_DOWN";
    case AppCode::key_up:
        return "KEY_UP";
    case AppCode::aim_event:
        return "AIM_EVENT";
    case AppCode::fire_event:
        return "FIRE_EVENT";
    case AppCode::game_join_req:
        return "GAME_JOIN_REQ";
    case AppCode::game_start:
        return "GAME_START";
    case AppCode::game_state:
        return "GAME_STATE";
    case AppCode::app_ack:
        return "APP_ACK";
    case AppCode::game_move:
        return "GAME_MOVE";
    case AppCode::game_target:
        return "GAME_TARGET";
    case AppCode::game_shoot:
        return "GAME_SHOOT";
    default:
        return "UNKNOWN_APP_CODE";
    }
}
} // namespace cyber
