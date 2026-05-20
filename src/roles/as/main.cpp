#include "cyber/shared/role_runtime.hpp"

// AS 可执行文件入口，委托统一角色运行时解析参数并启动 AS 服务。
int main(int argc, char** argv)
{
    return cyber::run_role_main(cyber::RoleKind::as_server, argc, argv);
}
