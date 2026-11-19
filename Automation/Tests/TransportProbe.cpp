// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#include <cymcp/mcp.hpp>
#include <fstream>
#include <iostream>
#include <thread>
using namespace cymcp;
int wmain(int argc, wchar_t **argv)
{
    try
    {
        if (argc != 2)
            throw Error("TestFailed", "Expected a private instance descriptor");
        std::ifstream file(argv[1]);
        Json descriptor;
        file >> descriptor;
        McpClient client(descriptor.at("url"), descriptor.at("token"));
        client.connect();
        auto require = [](bool condition, const char *message) {
            if (!condition)
                throw Error("TestFailed", message);
        };
        require(!client.list_tools().empty(), "Missing discovery");
        client.list_tools();
        require(client.cache_stats().hits > 0, "Discovery was not cached");
        client.start_notifications();
        require(client.notification_status().value("running", false), "HTTP subscription did not start");
        McpClient second(descriptor.at("url"), descriptor.at("token"));
        second.connect();
        second.start_notifications();
        client.clear_cache();
        Cancellation bounded;
        bounded.deadline = Clock::now() + std::chrono::seconds(3);
        require(!client.list_tools(bounded).empty(), "Two subscribers blocked business discovery");
        const auto resources = client.list_resources();
        require(!resources.empty(), "Missing registered resources");
        client.read_resource("cygc://state");
        client.read_resource("cygc://state");
        const auto misses = client.cache_stats().misses;
        Json events = Json::array();
        for (int i = 0; i < 70 && events.empty(); ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            for (const auto &event : client.drain_notifications())
                if (event.value("method", std::string()) == "notifications/resources/list_changed" ||
                    event.value("method", std::string()) == "notifications/resources/updated")
                    events.push_back(event);
        }
        require(!events.empty(), "No live resource invalidation notification");
        client.list_resources();
        require(client.cache_stats().misses > misses, "Notification did not invalidate cached discovery before TTL");
        client.clear_cache();
        client.read_resource("cygc://state");
        client.stop_notifications();
        second.stop_notifications();
        std::cout << Json({{"passed", true},
                           {"cache_hits", client.cache_stats().hits},
                           {"notifications", events.size()}})
                         .dump()
                  << '\n';
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
