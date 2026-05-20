#pragma once

namespace cyber
{
// 可通过统一运行入口启动的角色类型。
enum class RoleKind
{
    as_server,
    tgs_server,
    v_server,
    client
};

// 各角色 main.cpp 的公共入口，负责解析参数、加载配置并启动对应服务。
int run_role_main(RoleKind role, int argc, char** argv);
} // namespace cyber
