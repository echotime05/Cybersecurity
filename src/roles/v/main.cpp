#include "cyber/shared/role_runtime.hpp"

// V 可执行文件入口，委托统一角色运行时解析参数并启动 V 游戏服务。
int main(int argc, char** argv)
{
    return cyber::run_role_main(cyber::RoleKind::v_server, argc, argv);
}
