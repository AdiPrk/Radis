// client_main.cpp
#include <enet/enet.h>
#include <iostream>
#include <string>

int main()
{
    // Initialize ENet
    if (enet_initialize() != 0)
    {
        std::cerr << "Failed to initialize ENet.\n";
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    // Create client host
    ENetHost* client = enet_host_create(
        nullptr, // create a client host
        1,       // allow 1 connection
        1,       // one channel
        0, 0
    );

    if (!client)
    {
        std::cerr << "Failed to create ENet client.\n";
        return EXIT_FAILURE;
    }

    // Server address
    ENetAddress address;
    enet_address_set_host(&address, "45.61.62.97");
    address.port = 7777;

    // Try connecting
    ENetPeer* peer = enet_host_connect(client, &address, 1, 0);
    if (!peer)
    {
        std::cerr << "No available peers for initiating connection.\n";
        enet_host_destroy(client);
        return EXIT_FAILURE;
    }

    std::cout << "Connecting to server...\n";

    ENetEvent event;
    // Wait 5 seconds for connection event
    if (enet_host_service(client, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        std::cout << "Connected successfully!\n";
    }
    else
    {
        std::cerr << "Connection failed.\n";
        enet_peer_reset(peer);
        enet_host_destroy(client);
        return EXIT_FAILURE;
    }

    enet_host_flush(client);

    // Immediately disconnect cleanly
    enet_peer_disconnect(peer, 0);

    // Pump disconnect event for up to 3 seconds
    while (enet_host_service(client, &event, 3000) > 0)
    {
        if (event.type == ENET_EVENT_TYPE_DISCONNECT)
        {
            break;
        }
    }

    enet_host_destroy(client);
    return EXIT_SUCCESS;
}
