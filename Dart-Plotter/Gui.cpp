#include "Gui.h"
#include "font.h"
#include "ImGui/imgui_internal.h"

Gui Gui::g_Instance;

Gui::Gui() : szTitle(L"Gui - DartPlotter"), szWindowClass(L"NUXAR_WND") {}

int Gui::StartWindow()
{
	if (!MyRegisterClass())
	{
		return GetLastError();
	}

	if (!InitInstance())
	{
		return GetLastError();
	}
	MSG msg;
	BOOL bRet;
	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			return -1;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}

ATOM Gui::MyRegisterClass()
{
	WNDCLASSEX windowClass;

	ZeroMemory(&windowClass, sizeof(windowClass));

	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = GetModuleHandle(0);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)RGB(0, 0, 0);
	windowClass.lpszClassName = szWindowClass;

	return RegisterClassEx(&windowClass);
}

BOOL Gui::InitInstance()
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);

	HWND hWnd = CreateWindowEx(WS_EX_TRANSPARENT, szWindowClass, szTitle, WS_POPUP | WS_VISIBLE,
		desktop.right / 2 - 250, desktop.bottom / 2 - 350, 400, 500, nullptr, nullptr, GetModuleHandle(0), nullptr);

	int test = GetLastError();
	if (!hWnd)
	{
		return FALSE;
	}
	SetWindowLong(hWnd, GWL_EXSTYLE, WS_EX_TRANSPARENT);

	const MARGINS margins = { -1 };
	DwmExtendFrameIntoClientArea(hWnd, &margins);//make transparent
	hWindow = hWnd;

	xInitD3d();
	CreateRenderTarget();

	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
	SetupStyle();
	//ImGui fonts
	ImGuiIO& io = ImGui::GetIO();
	ImFont* m_pFont = io.Fonts->AddFontFromMemoryTTF(font, sizeof(font), 22, nullptr,
		io.Fonts->GetGlyphRangesCyrillic());
	SetForegroundWindow(hWnd);
	UpdateWindow(hWnd);
	return TRUE;
}

void Gui::xInitD3d()
{
	DXGI_SWAP_CHAIN_DESC sd;
	{
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 0;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWindow;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	}

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[1] = { D3D_FEATURE_LEVEL_11_0, };
	//XGUARD_HR(
	D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 1,
		D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);// != S_OK);
}

void Gui::CreateRenderTarget()
{
	DXGI_SWAP_CHAIN_DESC sd;
	g_pSwapChain->GetDesc(&sd);

	ID3D11Texture2D* pBackBuffer;
	D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
	ZeroMemory(&render_target_view_desc, sizeof(render_target_view_desc));
	render_target_view_desc.Format = sd.BufferDesc.Format;
	render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, &render_target_view_desc, &g_mainRenderTargetView);
	g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
	pBackBuffer->Release();
}

void Gui::CleanupRenderTarget()
{
	if (g_mainRenderTargetView)
	{
		g_mainRenderTargetView->Release();
		g_mainRenderTargetView = NULL;
	}
}

bool Gui::FileDialog(std::wstring& out_path, bool save_file)
{
	bool success = false;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog* pFileOpen;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(save_file ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			save_file ? IID_IFileSaveDialog : IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{
			// Show the Open dialog box.
			hr = pFileOpen->Show(hWindow);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr))
			{
				IShellItem* pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					if (SUCCEEDED(hr))
					{
						out_path = std::wstring(pszFilePath);
						success = true;
						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}
	return success;
}

bool Gui::LoadTextureFromMemory(ID3D11ShaderResourceView** out_srv, cv::Mat image)
{
	cv::Mat converted;
	cv::cvtColor(image, converted, cv::COLOR_BGR2BGRA, 4);

	// Load from disk into a raw RGBA buffer
	int image_width = image.cols;
	int image_height = image.rows;
	//unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
	if (converted.size == NULL)
		return false;

	// Create texture
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = image_width;
	desc.Height = image_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	ID3D11Texture2D* pTexture = NULL;
	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = converted.data;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	//srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
	pTexture->Release();
	//stbi_image_free(image_data);

	return true;
}

void Gui::SolveFile()
{
	std::wstring file_path;
	if (FileDialog(file_path)) {
		mutex_data.lock();
		image_detector = std::make_unique<ImageDetection>(file_path);
		if (image_detector->init()) {
			D3DX11_IMAGE_LOAD_INFO info;
			ID3D11Resource* resource;
			HRESULT result;
			image = std::make_unique<ImageTexture>();
			LoadTextureFromMemory(&image->texture, image_detector->image);
			//D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, image_detector->image.data(), image_detector->image.size(), &info, NULL, &image->texture, &result);
			image->size = { (float)image_detector->image.cols, (float)image_detector->image.rows };
		}
		else
			image_detector = std::unique_ptr<ImageDetection>();
		mutex_data.unlock();
	}
}

void Gui::SetupStyle() {
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.00f;
	style.WindowPadding = ImVec2(20, 14);
	style.WindowMinSize = ImVec2(400, 450);
	style.WindowRounding = 8; //4.0 for slight curve
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.ChildRounding = 8;
	style.FramePadding = ImVec2(5, 5);
	style.FrameRounding = 8; //2.0
	style.ItemSpacing = ImVec2(8, 4);
	style.ItemInnerSpacing = ImVec2(4, 4);
	style.TouchExtraPadding = ImVec2(0, 0);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 3.0f;
	style.ScrollbarSize = 10.0f;
	style.ScrollbarRounding = 6; //16.0
	style.GrabMinSize = 0.1f;
	style.GrabRounding = 8; //16.00
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.DisplayWindowPadding = ImVec2(22, 22);
	style.DisplaySafeAreaPadding = ImVec2(4, 4);
	style.AntiAliasedLines = true;
	style.CurveTessellationTol = 1.25f;
	style.PopupRounding = 8;
	style.TabRounding = 8;
	// Setup style
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 1);
	colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.37f, 0.14f, 0.14f, 0.67f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.39f, 0.20f, 0.20f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.56f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.19f, 0.19f, 0.50f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.89f, 0.00f, 0.19f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(1.00f, 0.19f, 0.19f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.80f, 0.17f, 0.00f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.89f, 0.00f, 0.19f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.33f, 0.35f, 0.36f, 0.53f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.76f, 0.28f, 0.44f, 0.67f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.47f, 0.47f, 0.47f, 0.67f);
	colors[ImGuiCol_Separator] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
	colors[ImGuiCol_Tab] = ImVec4(0.07f, 0.07f, 0.07f, 0.51f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.23f, 0.43f, 0.67f);
	colors[ImGuiCol_TabActive] = ImVec4(0.19f, 0.19f, 0.19f, 0.57f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.05f, 0.05f, 0.05f, 0.90f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.13f, 0.13f, 0.13f, 0.74f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void Gui::DrawRect(ImVec2 curser_pos, std::vector<cv::Rect>::iterator r, float factor, ImU32 color)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	window->DrawList->AddRect({ curser_pos.x + r->x * factor, curser_pos.y + r->y * factor }, { curser_pos.x + (r->x + r->width) * factor, curser_pos.y + (r->y + r->height) * factor },
		color, 1, 0, 1);
}

void Gui::DrawImageWithDetection()
{
	bool remove_buttons = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
	ImVec2 curser_pos = ImGui::GetCursorPos();
	curser_pos.y -= ImGui::GetScrollY();

	float avail_width = ImGui::GetWindowWidth() - curser_pos.x;
	float factor = avail_width / image->size.x;
	ImGui::Image(image->texture, { image->size.x * factor, image->size.y * factor });
	for (int i = 0; i < image_detector->holes.size(); i++)
	{
		auto r = image_detector->holes.begin() + i;
		if (remove_buttons) {
			char button_name[256];
			sprintf_s(button_name, "##%p%d", (void*)r._Ptr, r->x);
			if (ImGui::CloseButton(ImGui::GetCurrentWindow()->GetID(button_name), { curser_pos.x + r->x * factor, curser_pos.y + r->y * factor }, ImGui::GetColorU32(ImGuiCol_FrameBgActive))) {
				RemoveIfExsists(r, image_detector->holes);
				image_detector->LoadDistances();
				return;
			}
		}
		DrawRect(curser_pos, r, factor, ImGui::GetColorU32({ 1, 0, 0, 1 }));
	}

	for (int i = 0; i < image_detector->center.size(); i++)
	{
		auto r = image_detector->center.begin() + i;
		if (remove_buttons) {
			char button_name[256];
			sprintf_s(button_name, "##%p%d", (void*)r._Ptr, r->x);
			if (ImGui::CloseButton(ImGui::GetCurrentWindow()->GetID(button_name), { curser_pos.x + r->x * factor, curser_pos.y + r->y * factor }, ImGui::GetColorU32(ImGuiCol_FrameBgActive))) {
				RemoveIfExsists(r, image_detector->center);
				image_detector->LoadDistances();
				return;
			}
		}
		DrawRect(curser_pos, r, factor, ImGui::GetColorU32({ 1, 1, 0, 1 }));
	}

	auto first_center = image_detector->center.begin();
	if (first_center != image_detector->center.end()) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		window->DrawList->AddCircle({ curser_pos.x + (first_center->x + first_center->width / 2) * factor, curser_pos.y + (first_center->y + first_center->height / 2) * factor }, factor * 20, ImGui::GetColorU32({ 1, 1, 0, 1 }), 10, 3);
	}
}

void Gui::Render()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::SetNextWindowSizeConstraints(ImVec2(450, 250), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	bool open = true;
	bool maximize = true;
	ImGui::Begin(_bstr_t(szTitle), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MoveHWND, &maximize);
	if (!maximize) {
		RECT desktop;
		const HWND hDesktop = GetDesktopWindow();
		GetWindowRect(hDesktop, &desktop);
		MoveWindow(hWindow, 0, 0, desktop.right, desktop.bottom, true);
		ImGui::SetWindowSize(ImVec2(desktop.right, desktop.bottom), ImGuiCond_Always);
	}
	if (!open)
		PostQuitMessage(0);

	{
		if (ImGui::Button("Load file")) {
			if (!open_file_task.valid() || open_file_task.wait_for(std::chrono::seconds(0)) != std::future_status::timeout)
				open_file_task = std::async(std::launch::async, &Gui::SolveFile, this);
		}
		if (mutex_data.try_lock()) {
			if (image_detector.get()) {
				ImGui::SameLine();
				ImGui::Text("Press ctrl to delete detections (e.g if they're detected 2 times wrongly)");
				DrawImageWithDetection();

				ImGui::PlotHistogram("##dstToMiddle", [](void* data, int idx) -> float {
					try
					{
						std::map<float, int>* map = (std::map<float, int>*)data;
						return(*map)[idx * 50];
					}
					catch (const std::exception&)
					{
						return 0;
					};
					}, &image_detector->distance_counts, 40, 0, 0, FLT_MAX, FLT_MAX, ImVec2(ImGui::GetContentRegionAvailWidth(), 200), 50);
				if (ImGui::Button("Write file")) {
					auto save_file = [&]() {std::wstring path;
					if (FileDialog(path, true)) {
						std::ofstream file(path);
						for (size_t i = 0; i < image_detector->distances.size(); i++)
						{
							file << image_detector->distances[i] << (i % 3 == 2 ? "\n" : "\t\t");
						}
						file.close();
					}
					};
					if (!open_file_task.valid() || open_file_task.wait_for(std::chrono::seconds(0)) != std::future_status::timeout)
						open_file_task = std::async(std::launch::async, save_file);
				}
			}
			mutex_data.unlock();
		}
		else {
			ImGui::BufferingBar("##progress", 1, ImVec2(ImGui::GetWindowWidth(), 8), ImGui::GetColorU32(ImVec4(0.47f, 0.47f, 0.47f, 0.67f)), ImGui::GetColorU32(ImVec4(0.89f, 0.00f, 0.19f, 1.00f)));
		}
	}

	ImGui::End();
	ImGui::EndFrame();
	const static float clearColor[] = { 0, 0, 0, 0 };
	g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (const float*)(clearColor));
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	g_pSwapChain->Present(0, 0);
	Sleep(1);
}


extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return 0;

	switch (message)
	{
	case WM_PAINT:
		Gui::Get().Render();
		break;
	case WM_SIZE:
		if (Gui::Get().g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX11_InvalidateDeviceObjects();

			Gui::Get().CleanupRenderTarget();
			Gui::Get().g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			Gui::Get().CreateRenderTarget();

			ImGui_ImplDX11_CreateDeviceObjects();
		}
		return 0;
	case WM_DESTROY:
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}