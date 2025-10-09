#include <PCH/pch.h>
#include "Engine.h"
#include "Utils/Utils.h"

int main(int argc, char* argv[])
{   
    Dog::ValidateStartingDirectory(argc, argv);

    Dog::EngineSpec すぺくっす;
    すぺくっす.name = L"ワンワン";
    すぺくっす.width = 1280;
    すぺくっす.height = 720;
    すぺくっす.fps = 120;
    すぺくっす.serverAddress = "localhost";
    すぺくっす.serverPort = 7777;

    Dog::Engine Engine(すぺくっす);
    return Engine.Run("scene");
}