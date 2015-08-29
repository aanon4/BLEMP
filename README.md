# Bluetooth Low Energy Mesh Protocol (BLEMP) demo

The Bluetooth Low Energy Mesh Protocol is written in C and built to link a randomly distributed set of Bluetooth nodes.
These nodes form a mesh network, allowing data to be send and received between nodes which are not directly
connected. The protocol is designed for battery operated devices, using as little as 20uA per node during normal operation.

# Nordic nRF51822 implementation

The demo app is design to work on the Nordic nRF51822 chip (https://www.nordicsemi.com/eng/Products/Bluetooth-Smart-Bluetooth-low-energy) using the S130 softdevice. The project is built with Eclipse (http://embeddedsoftdev.blogspot.ca/p/eclipse.html) and can be easily run on a Nordic nRF51 demo board (http://components.arrow.com/part/detail/69839820S1083443N7340) or other hardware.

# Simulator

An Xcode Mesh simulator is also included. This allows for easy testing of the protocol.

# The Code

The BLEMP demo consists of three parts:

1. A temperature monitor.
2. A bluetooth beacon. This allows appropriate client software to know its location in relation to sensors.
3. The bluetooth mesh network. The temperature readings are sent through the mesh so any node can report
   the temperature of any other node.
4. Time synchronization to keep all the nodes on the same global time - think NTP for your mesh network.
