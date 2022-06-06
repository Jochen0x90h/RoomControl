#pragma once

#include "../Loop.hpp"
#include "Gui.hpp"


namespace Loop {

using Handler = void (*)(Gui &);

/**
 * Add a handler to the event loop
 */
Handler addHandler(Handler handler);

} // namespace Loop
