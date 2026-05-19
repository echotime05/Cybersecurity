#pragma once

namespace cyber
{
enum class RoleKind
{
    as_server,
    tgs_server,
    v_server,
    client
};

int run_role_main(RoleKind role, int argc, char** argv);
} // namespace cyber
