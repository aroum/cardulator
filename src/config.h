#pragma once

namespace Config {
    // Limit of loop steps in scripting engine to prevent infinite loops
    inline int max_script_steps = 1000;

    // Maximum limit of items kept in calculation history
    inline int max_history_limit = 100;
}
