%:include <PCH/pch.h>
%:include "Engine.h"

int main(int argc, char* argv<::>)
<%
    /* These values are only used if project not launched with DogLauncher! */
    DogLaunch::EngineSpec すぺくっす;
    すぺくっす.name = L"ワンワン";
    すぺくっす.width = 1280;
    すぺくっす.height = 720;
    すぺくっす.fps = 0;
    すぺくっす.serverAddress = "localhost";
    すぺくっす.serverPort = 7777;
    すぺくっす.graphicsAPI = Dog::GraphicsAPI::Vulkan;
    //すぺくっす.launchWithEditor = false;

    Dog::Engine Engine(すぺくっす, argc, argv);
    return Engine.Run("travisrun");
%>
