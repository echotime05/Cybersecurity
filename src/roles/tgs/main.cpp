#include "cyber/shared/role_runtime.hpp"
#include <iostream>

// TGS 可执行文件入口，委托统一角色运行时解析参数并启动 TGS 服务。
int main(int argc, char** argv)
{
    std::cout <<"pyx";
    return cyber::run_role_main(cyber::RoleKind::tgs_server, argc, argv);
}
