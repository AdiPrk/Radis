// main.cpp
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

// --- Forward Declarations & Placeholder Functions ---
void RefreshProjectList(std::vector<std::string>& projectNames);
void LaunchApplication(const std::string& projectName);

// --- Main Application Function ---
void RunImGuiApplication()
{
    // 1. SETUP (GLFW, OpenGL Context, GLEW, ImGui)
    // ------------------------------------------------
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    // Set OpenGL version (e.g., 3.3)
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create a window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dog Launcher", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    glfwSetWindowSizeLimits(window, 100, 50, GLFW_DONT_CARE, GLFW_DONT_CARE);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- State Variables ---
    static std::vector<std::string> projectNames;
    static int selectedProject = -1;
    static bool showCreateProject = false;


    // 2. MAIN LOOP
    // ------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        ImVec2 windowSize = ImGui::GetMainViewport()->Size;
        ImVec2 windowPos = ImGui::GetMainViewport()->Pos;
        ImGui::SetNextWindowSize(windowSize);
        ImGui::SetNextWindowPos(windowPos);

        ImGui::Begin("Dog Launcher", nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar);

        float leftPanelWidth = windowSize.x * 0.3f;
        float rightPanelWidth = windowSize.x - leftPanelWidth;

        ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, 0), true);
        {
            RefreshProjectList(projectNames);
            ImGui::Text("Recent Projects");
            ImGui::Separator();
            for (int i = 0; i < projectNames.size(); ++i) {
                if (ImGui::Selectable(projectNames[i].c_str(), selectedProject == i)) {
                    selectedProject = i;
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("Create New Project", ImVec2(-1, 40))) {
                showCreateProject = true;
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth - 20, 0), true);
        {
            if (selectedProject != -1 && selectedProject < projectNames.size()) {
                ImGui::Text("Project Details");
                ImGui::Separator();
                ImGui::Text("Details for Project: %s", projectNames[selectedProject].c_str());
                ImGui::Spacing();
                if (ImGui::Button("Load Project", ImVec2(200, 40))) {
                    LaunchApplication(projectNames[selectedProject]);
                }
            }
            else {
                ImGui::Text("Welcome to Dog Launcher");
                ImGui::Spacing();
                ImGui::TextWrapped("Select a project from the list or create a new one to get started.");
            }
        }
        ImGui::EndChild();

        if (showCreateProject) {
            ImGui::OpenPopup("Create New Project");
            showCreateProject = false;
        }
        if (ImGui::BeginPopupModal("Create New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char newProjectName[128] = "";
            static bool invalidProjectName = false, projectAlreadyExists = false, reservedProjectName = false;

            ImGui::InputText("Project Name", newProjectName, IM_ARRAYSIZE(newProjectName));
            if (invalidProjectName) ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Project name cannot be empty!");
            if (reservedProjectName) ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Project name 'base' or 'dev' is reserved!");
            if (projectAlreadyExists) ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "A project with this name already exists!");

            ImGui::Spacing();
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                invalidProjectName = projectAlreadyExists = reservedProjectName = false;
                std::string pNameStr(newProjectName);
                std::string clientDir = "../Client/";
                std::filesystem::path projectPath = clientDir + pNameStr;
                std::string pNameLower = pNameStr;
                std::transform(pNameLower.begin(), pNameLower.end(), pNameLower.begin(), ::tolower);

                if (strlen(newProjectName) == 0) invalidProjectName = true;
                else if (pNameLower == "base" || pNameLower == "dev") reservedProjectName = true;
                else if (std::filesystem::exists(projectPath)) projectAlreadyExists = true;
                else {
                    try {
                        std::filesystem::create_directory(projectPath);
                        std::filesystem::copy("Base", projectPath, std::filesystem::copy_options::recursive);
                        LaunchApplication(pNameStr);
                        ImGui::CloseCurrentPopup();
                    }
                    catch (const std::filesystem::filesystem_error& e) {
                        std::cerr << "Filesystem error: " << e.what() << std::endl;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        glfwSwapBuffers(window);
    }

    // 3. CLEANUP
    // ------------------------------------------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int argc = __argc;
    char** argv = __argv;
    for (int i = 1; i < argc; ++i) 
    {
        std::string arg = argv[i];
        if (arg == "dev")
        {
            MessageBoxA(nullptr, "Cannot run launcher directly through Visual Studio. Run the exe from the output directory.", "Error", MB_OK | MB_ICONERROR);
            return -1;
        }
    }

    RunImGuiApplication();
    return 0;
}

// --- Placeholder Function Implementations ---
void RefreshProjectList(std::vector<std::string>& projectNames) 
{
    projectNames.clear();
    std::string clientDir = "../Client/";

    // Iterate through all directories in "../Client/"
    for (const auto& entry : std::filesystem::directory_iterator(clientDir))
    {
        if (entry.is_directory())  // Only add directories (projects)
        {
            std::string projectName = entry.path().filename().string();
            if (projectName == "Base" || projectName == "Dev") continue;

            projectNames.push_back(projectName);
        }
    }
}

void LaunchApplication(const std::string& projectName)
{
    std::string clientDir = "../Client/";
    std::string exeName = "Dog.exe";

    // Convert clientDir to an absolute path.
    std::filesystem::path absoluteClientDir = std::filesystem::absolute(clientDir);
    // Form the full path to the executable.
    std::filesystem::path exePath = absoluteClientDir / exeName;

    // Create the command line for Dog.exe.
    std::string command = "\"" + exePath.string() + "\" -projectdir " + projectName;
    
    // Convert the command line to a wide string for CreateProcessW.
    std::wstring wideCommand(command.begin(), command.end());

    // Also convert the client directory (which is where the exe resides) to a wide string.
    std::wstring wideWorkingDir = absoluteClientDir.wstring();

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Launch Dog.exe in a separate process, using the client directory as the working directory.
    if (CreateProcessW(
        NULL,                                   // No application name (using command line)
        &wideCommand[0],                        // Command line
        NULL,                                   // Process handle not inheritable
        NULL,                                   // Thread handle not inheritable
        FALSE,                                  // Set handle inheritance to FALSE
        CREATE_NEW_CONSOLE,                     // Run in a new console window
        NULL,                                   // Use parent's environment block
        wideWorkingDir.empty() ? NULL : wideWorkingDir.c_str(), // Use client dir as working directory
        &si,                                    // Pointer to STARTUPINFOW structure
        &pi                                     // Pointer to PROCESS_INFORMATION structure
    ))
    {
        // Close process and thread handles (we don't need them)
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        MessageBoxA(nullptr, "Failed to start Dog.exe! Make sure the Client directory and Dog.exe exist.", "Error", MB_OK | MB_ICONERROR);
    }

    //MessageBoxA(nullptr, "The project has been loaded. Please close the launcher.", "Project Loaded", MB_OK | MB_ICONINFORMATION);
}