#include "global.hpp"


namespace global {

asio::io_context context;

asio::ip::udp::endpoint local;
asio::ip::udp::endpoint upLink;
asio::ip::udp::endpoint downLocal;

Gui *gui;

}
