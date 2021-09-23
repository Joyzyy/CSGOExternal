#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_dx9.h"
#include "../ImGui/imgui_impl_win32.h"

#include <tchar.h>
#include <dinput.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d9.lib")
#include <d3d9.h>
#include "Dwmapi.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Data {
    LPDIRECT3D9             g_pD3D = NULL;
    LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
    D3DPRESENT_PARAMETERS   g_d3dpp = {};
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (Data::g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            Data::g_d3dpp.BackBufferWidth = LOWORD(lParam); Data::g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ImGui_ImplDX9_InvalidateDeviceObjects();
            if (HRESULT hResult = Data::g_pd3dDevice->Reset(&Data::g_d3dpp); hResult == D3DERR_INVALIDCALL) { IM_ASSERT(0); }
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) { return 0; } break;
    
    case WM_NCHITTEST: {
        ImVec2 vec2D = ImGui::GetMousePos(); if (vec2D.y < 25 && vec2D.x < 1280 - 25) {
            LRESULT lResult = ::DefWindowProc(hWnd, msg, wParam, lParam); if (lResult == HTCLIENT) { lResult = HTCAPTION; } return lResult;
        } else { break; }
    }

    case WM_DESTROY:
        ::PostQuitMessage(0); return 0;
    
    default:
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam); return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}