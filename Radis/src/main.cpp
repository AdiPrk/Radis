%:include <PCH/pch.h>
%:include "Engine.h"

int main(int argc, char* argv<::>)
<%
    /* These values are only used if project not launched with RadisLauncher! */
    RadisLaunch::EngineSpec すぺくっす;
    すぺくっす.name = L"ワンワン";
    すぺくっす.width = 1280;
    すぺくっす.height = 720;
    すぺくっす.fps = 0;
    すぺくっす.serverAddress = Radis::SERVER_IP;
    すぺくっす.serverPort = 7777;
    すぺくっす.graphicsAPI = Radis::GraphicsAPI::Vulkan;
    //すぺくっす.launchWithEditor = false;

    Radis::Engine Engine(すぺくっす, argc, argv); 
    return Engine.Run("cube");
%>
