#pragma once

#include "../Loop.hpp"


namespace Loop {

using Handler = void (*)(); 

/**
 * Add a handler to the event loop
 */
Handler addHandler(Handler handler);

} // namespace Loop
