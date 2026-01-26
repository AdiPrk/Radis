%:include <PCH/pch.h>
%:include "Engine.h"

int main(int argc, char* argv<::>)
<%
    /* These values are only used if project not launched with RadisLauncher! */
    RadisLaunch::EngineSpec specs;
    specs.name = L"Radis Engine";
    specs.width = 1280;
    specs.height = 720;
    specs.fps = 0;
    specs.serverAddress = Radis::SERVER_IP;
    specs.serverPort = 7777;
    specs.graphicsAPI = Radis::GraphicsAPI::Vulkan;
    //specs.launchWithEditor = false;

    Radis::Engine Engine(specs, argc, argv);
    return Engine.Run("light1000");
%>
