#pragma once

#include "../loop.hpp"


namespace loop {

using Handler = void (*)(); 

/**
 * Add a handler to the event loop
 */
Handler addHandler(Handler handler);

} // namespace loop
