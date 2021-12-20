#pragma once
#include <Windows.h>
#include <Windowsx.h>
#include <shobjidl.h> 
#include <Dwmapi.h>
#include <comdef.h>  // for WCHAR to CHAR conversion
#include <map>
#include <vector>
#include <string>
#include <future>
#include <mutex>
#include <fstream>
#include <d3d11.h>
#include <d3dx11.h>
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImageDetection.h"

#define MAX_LOADSTRING 100
struct ImageTexture
{
	ID3D11ShaderResourceView* texture;
	ImVec2 size;
};
class Gui //this class is a singleton. There will always only be 1 window.
{
private:
	/// <summary>
	/// Used for protecting data shared between threads. Makes it so it's not accessed simultaneously.
	/// </summary>
	std::mutex mutex_data;

	/// <summary>
	/// Access to the async operation. Needed e.g for checking if there's already a file being solved/loaded.
	/// </summary>
	std::future<void> open_file_task;

	/// <summary>
	/// Input text for weight lookup.
	/// </summary>
	char search_input[200];

	std::unique_ptr<ImageTexture> image;
	std::unique_ptr<ImageDetection> image_detector;

	/// <summary>
	/// Handle to the window.
	/// </summary>
	HWND hWindow;

	/// <summary>
	/// Window title.
	/// </summary>
	WCHAR szTitle[MAX_LOADSTRING];

	/// <summary>
	/// Window class name.
	/// </summary>
	WCHAR szWindowClass[MAX_LOADSTRING];


	/// <summary>
	/// For d3d11.
	/// </summary>
	ID3D11Device* g_pd3dDevice = 0;

	/// <summary>
	/// For d3d11.
	/// </summary>
	ID3D11DeviceContext* g_pd3dDeviceContext = 0;

	/// <summary>
	/// For d3d11.
	/// </summary>
	IDXGISwapChain* g_pSwapChain = 0;

	/// <summary>
	/// For d3d11.
	/// </summary>
	ID3D11RenderTargetView* g_mainRenderTargetView = 0;
public:
	Gui(const Gui&) = delete;
	/// <summary>
	/// Singleton getter function
	/// </summary>
	/// <returns>The only instance of this class</returns>
	static Gui& Get() {
		return g_Instance;
	}

	/// <summary>
	/// The main render function. It draws every frame
	/// </summary>
	void Render();

	/// <summary>
	/// Starts the Gui window
	/// </summary>
	/// <returns>The last error (if the window creation failed) or the close reason of the window</returns>
	int StartWindow();
private:
	/// <summary>
	/// Initialized the window name
	/// </summary>
	Gui();

	template<typename T>
	inline void RemoveIfExsists(std::vector<cv::Rect>::iterator index, std::vector<T>& vector) {
		vector.erase(index);
	}

	void DrawRect(ImVec2 curser_pos, std::vector<cv::Rect>::iterator r, float factor, ImU32 color);

	void DrawImageWithDetection();

	/// <summary>
	/// Registers the window class
	/// </summary>
	/// <returns>If the function succeeds, the return value is a class atom that uniquely identifies the class being registered. This atom can only be used by the CreateWindow, CreateWindowEx, GetClassInfo, GetClassInfoEx, FindWindow, FindWindowEx, and UnregisterClass functions and the IActiveIMMap::FilterClientWindows method.</returns>
	ATOM MyRegisterClass();

	/// <summary>
	/// Creates the window then initializes d3d11 then ImGui.
	/// After that it sets the ImGui style, loads the font and lastly pushes the window to the front.
	/// </summary>
	/// <returns>Returns if the window was created successfully</returns>
	BOOL InitInstance();

	/// <summary>
	/// Initializes d3d11
	/// </summary>
	void xInitD3d();

	/// <summary>
	/// Creates the d3d11 render target.
	/// Needed for drawing.
	/// </summary>
	void CreateRenderTarget();

	/// <summary>
	/// Cleans up the d3d11 render target
	/// This is needed to that the window is resized and not stretched.
	/// </summary>
	void CleanupRenderTarget();

	/// <summary>
	/// Opens the windows open file dialog.
	/// </summary>
	/// <param name="out_path">Text of the file path</param>
	/// <returns>If a file has been opened successfully</returns>
	bool FileDialog(std::wstring& out_path, bool save_file = false);

	bool LoadTextureFromMemory(ID3D11ShaderResourceView** out_srv, cv::Mat mat);

	/// <summary>
	/// Asks the user for the file and then solves it. 
	/// The function should be called async so that the window will still be drawn (not stuck on screen)
	/// </summary>
	void SolveFile();

	/// <summary>
	/// Sets up the Dear ImGui style (red)
	/// </summary>
	void SetupStyle();


	/// <summary>
	/// Microsoft documentation about window callbacks <a href="http://docs.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc">HERE</a>
	/// </summary>
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	/// <summary>
	/// The instance of this class (<a href="https://www.youtube.com/watch?v=PPup1yeU45I">whats a singleton?</a>)
	/// </summary>
	static Gui g_Instance;
};
