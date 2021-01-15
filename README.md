RoomControl
===========
Smart home room controller based on nRF52840 with MQTT and and IEEE 802.15.4 radio support. 
Buttons and relays for lights and blinds can also be connected with a local bus using 
LIN physical layer.

Control
-------
This is a 3D printed box with a display and digital potentiometer that gets installed in each
room instead of the floor heating thermostat. It can control the heating valve using an opto-triac,
connect to light and blind switches and relays using LIN physical layer and to battery-free switches
(EnOcan) using an IEEE 802.15.4 radio interface.

Switch
------
This is a wall switch and actor for lights and blinds that can be mounted behind Eltako E-Design
frames and rockers. It connects to the Control via a bus based on LIN physical layer.

Gateway
-------
This is a hat for rpi or compatible single board computers that connects to multiple Controls
via a IEEE 802.15.4 radio network and to an MQTT broker such as mosquitto.

License
-------
This work is licensed under a
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: http://creativecommons.org/licenses/by-nc-sa/4.0/
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
