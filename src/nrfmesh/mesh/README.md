# Bluetooth Low Energy Mesh Protocol (BLEMP)

The goal of this Mesh is to provide a way to build a network between a set of Bluetooth 4.1 nodes in such a way to allow any node to communicate with any other node, or an external client, without having a direct connection. BLE devices allow this to be achieved using very little power. On prototype hardware a mesh node uses very little power, allowing battery powered nodes with multi-year lifetimes.

## Basic communication protocol

The Mesh consists of a random arrangement of nodes. Each node broadcasts its presence using the standard BLE advertising mechanism. These adverts allow nodes to discover each other and form a set of neighbors. Data within the mesh is held as Id/Key/Value triples. The Id is the MAC address of the target node, the Key a 16-bit identifier, and Value an arbitrary value. Whenever a value changes on a node, the change will flood from neighbor to neighbor until it is seen by all nodes in the Mesh.

BLEMP works on top of the standard Bluetooth 4.1 protocol stack. 4.1 is required because nodes must be capable of being both Master and Peripheral (although not simultaneously). Idle nodes acts as Peripherals, advertising their presence for others in the standard way. Only when a node has changes to propagate does it become a Master. As a Master it connects to its known neighbors to send these changes. Once complete, it returns to being a Peripheral.

Because it is likely that many nodes share many of the same neighbors, the Mesh actively prunes common paths as values move across the network. Over time, a network will optimize itself to reduce unnecessary communication.

## Power

The Mesh is designed for battery powered devices. By keeping the node as a Peripheral, with no active connections, for the majority of the time, power usage is minimized. Consequently, even in an active network, power consumption on prototype hardware is less than 100uA.

## Memory

Memory is a highly limited resource on many BLE chips. The Mesh uses approximately 7 bytes per node plus 7 bytes per value in the network. A 100 node network with 200 values would use approximately 2K of RAM.
