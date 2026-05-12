#include "cyber/common/role_runtime.hpp"

int main(int argc, char** argv)
{
    return cyber::run_role_main(cyber::RoleKind::as_server, argc, argv);
}
