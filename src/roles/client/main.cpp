#include "cyber/shared/role_runtime.hpp"

// Client 可执行文件入口，委托统一角色运行时解析参数并启动本地 UI 和游戏客户端。
int main(int argc, char** argv)
{
    return cyber::run_role_main(cyber::RoleKind::client, argc, argv);
}
