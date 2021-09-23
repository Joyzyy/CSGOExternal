#include "Basil/Basil.hh"
#include "Basil/StringToByteArray.hh"
#include "Menu/Menu.hh"
#include <iostream>
#include <stdexcept>

using namespace Basil; using namespace Data;

static bool keep_log = false;

int main(int argc, char* argv[]) {
    try {

        Ctx_t pObject("csgo.exe"); pObject.CaptureAllModules();
        auto ModClient = pObject.GetModule("client.dll").value();

    #pragma region("IMGUI IMPLEMENTATION")
        RECT scr; GetWindowRect(GetDesktopWindow(), &scr); 
        WNDCLASSEX wclassex = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("nullptr"), NULL }; ::RegisterClassEx(&wclassex);
        HWND hWnd = ::CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, _T("nullptr"), _T("nullptr"), WS_POPUP, (scr.right / 2) - (1280 / 2), (scr.bottom / 2) - (720 / 2), 1280, 720, 0, 0, wclassex.hInstance, 0);

        ::SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, ULW_COLORKEY);

        auto CreateDevice = [&](HWND hWnd) -> bool {
            if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) { return false; }

            ZeroMemory(&g_d3dpp, sizeof(g_d3dpp)); g_d3dpp.Windowed = TRUE; g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD; g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; g_d3dpp.EnableAutoDepthStencil = TRUE; g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16; g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
            if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0) { return false; }
        
            return true;
        }; auto CleanupDevice = [&]() -> void {
            if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; } if (g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; } 
        }; if (!CreateDevice(hWnd)) {
            CleanupDevice();
            ::UnregisterClass(wclassex.lpszClassName, wclassex.hInstance);
            throw std::runtime_error("Invalid hwnd value."); return 0;
        } ::ShowWindow(hWnd, SW_SHOWDEFAULT); ::UpdateWindow(hWnd); ::SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); (void)io; ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(hWnd); ImGui_ImplDX9_Init(g_pd3dDevice);

        MSG msg; ZeroMemory(&msg, sizeof(msg)); while (msg.message != WM_QUIT) {
            if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg); ::DispatchMessage(&msg); continue;
            } Sleep(20);

            ImGui_ImplDX9_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();

            static bool open = true; if (!open) { throw std::runtime_error("[bool] &open = false; ::ExitProcess(0) if !open"); ::ExitProcess(0); }

            ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_Once); ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            ImGui::Begin("nullptr", &open , ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse); {
                ImGui::Text("Name: "); ImGui::SameLine(); ImGui::Text(pObject.GetName().c_str());
                ImGui::Text("Pid value: "); ImGui::SameLine(); ImGui::Text((char*)(pObject.GetPID().value()));
                ImGui::Text("client.dll: "); ImGui::SameLine(); ImGui::Text((char*)(ModClient.m_nSize));
            } ImGui::End();

            ImGui::EndFrame();

            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.F, 0);
            if (g_pd3dDevice->BeginScene() >= 0) {
                ImGui::Render(); ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData()); g_pd3dDevice->EndScene();
            } if (HRESULT hResult = g_pd3dDevice->Present(NULL, NULL, NULL, NULL); hResult == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
                ImGui_ImplDX9_InvalidateDeviceObjects();
                if (HRESULT _hResult = g_pd3dDevice->Reset(&g_d3dpp); _hResult == D3DERR_INVALIDCALL) { IM_ASSERT(0); }
                ImGui_ImplDX9_CreateDeviceObjects();
            }
        }

        ImGui_ImplDX9_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); CleanupDevice();

        ::DestroyWindow(hWnd); ::UnregisterClass(wclassex.lpszClassName, wclassex.hInstance);
    #pragma endregion

    } catch (const std::exception& err) {
        std::cout << err.what() << std::endl;
        if (keep_log) system("pause");
        std::exit(EXIT_FAILURE);
    }
    return 0;
}