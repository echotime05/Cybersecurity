#include "cyber/shared/role_runtime.hpp"

int main(int argc, char** argv)
{
    return cyber::run_role_main(cyber::RoleKind::client, argc, argv);
}
