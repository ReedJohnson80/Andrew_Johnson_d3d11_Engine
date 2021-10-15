#include "Graphics.h"

bool Graphics::Initialize(HWND hwnd, int width, int height)
{
	fpsTimer.Start();
	this->windowWidth = width;
	this->windowHeight = height;

	if (!InitializeDirectX(hwnd))
		return false;

	if (!InitializeShaders())
		return false;

	if (!InitializeScene())
		return false;

	//Set up ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(this->device.Get(), this->deviceContext.Get());
	ImGui::StyleColorsDark();

	return true;
}

bool Graphics::InitializeDirectX(HWND hwnd)
{
	try
	{
		//INIT ADAPTER
		std::vector<AdapterData> adapters = AdapterReader::GetAdapters();
		if (adapters.size() < 1)
		{
			ErrorLogger::Log("No IDXGI Adapters found.");
			return false;
		}

		//Bubble sort adapters to put the adapter with the largest dedicated video memory at the front of the vector 
		int lastNdx = adapters.size() - 1;
		bool sorted = false;
		for (int i = 0; i < adapters.size() - 1; i++)
		{
			sorted = true;
			for (int j = 0; j < lastNdx; j++)
			{
				if (adapters[j].desc.DedicatedVideoMemory < adapters[j + 1].desc.DedicatedVideoMemory)
				{
					AdapterData temp = adapters[j];
					adapters[j] = adapters[j + 1];
					adapters[j + 1] = temp;
					sorted = false;
				}
			}
			if (sorted)
			{
				break;
			}
			lastNdx--;
		}

		//CREATE DEVICE AND SWAPCHAIN
		DXGI_SWAP_CHAIN_DESC scd = { 0 };
		scd.BufferDesc.Width = this->windowWidth;
		scd.BufferDesc.Height = this->windowHeight;
		scd.BufferDesc.RefreshRate.Numerator = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.BufferCount = 1;
		scd.OutputWindow = hwnd;
		scd.Windowed = TRUE;
		scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		HRESULT hr = D3D11CreateDeviceAndSwapChain(adapters[0].pAdapter, //IDXGI Adapter
			D3D_DRIVER_TYPE_UNKNOWN,
			NULL, //FFOR SOFTWARE DRIVER TYPE
			NULL, //FLAGS FOR RUNTIME LAYERS
			NULL, //FEATURE LEVELS ARRAY
			0, //NUMBER OF FEATURE LEVELS IN ARRAY
			D3D11_SDK_VERSION,
			&scd, //SWAPCHAIN DESCRIPTOR
			this->swapchain.GetAddressOf(), //SWAPCHAIN ADDRESS
			this->device.GetAddressOf(), //DEVICE ADDRESS
			NULL, //SUPPORTED FEATURE LEVEL
			this->deviceContext.GetAddressOf()); //DEVICE CONTEXT ADDRESS

		COM_ERROR_IF_FAILED(hr, "Failed to create device and swapchain.");

		//CREATE BACK BUFFER
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		hr = this->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
		COM_ERROR_IF_FAILED(hr, "Failed to get back buffer.");

		//CREATE RENDER TARGET VIEW
		hr = this->device->CreateRenderTargetView(backBuffer.Get(), NULL, this->renderTargetView.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create render target view.");

		//CREATE DEPTH STENCIL STATE AND VIEW
		CD3D11_TEXTURE2D_DESC depthStencilDesc(DXGI_FORMAT_D24_UNORM_S8_UINT, this->windowWidth, this->windowHeight);
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		hr = this->device->CreateTexture2D(&depthStencilDesc, NULL, this->depthStencilBuffer.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create depth stencil buffer.");
		hr = this->device->CreateDepthStencilView(this->depthStencilBuffer.Get(), NULL, this->depthStencilView.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create depth stencil view.");
		this->deviceContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), this->depthStencilView.Get());

		//CREATE DEPTH STENCIL STATE
		CD3D11_DEPTH_STENCIL_DESC depthStencilStateDesc(D3D11_DEFAULT);
		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;
		hr = this->device->CreateDepthStencilState(&depthStencilStateDesc, this->depthStencilState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create depth stencil state.");

		//CREATE AND SET VIEWPORT
		CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(this->windowWidth), static_cast<float>(this->windowHeight) );
		this->deviceContext->RSSetViewports(1, &viewport);

		//CREATE RASTERIZER STATE
		CD3D11_RASTERIZER_DESC rasterizerDesc(D3D11_DEFAULT);
		hr = this->device->CreateRasterizerState(&rasterizerDesc, this->rasterizerState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create rasterizer state.");

		//CREATE RASTERIZER STATE rs_cull_front
		CD3D11_RASTERIZER_DESC rs_cull_front_desc(D3D11_DEFAULT);
		rs_cull_front_desc.CullMode = D3D11_CULL_MODE::D3D11_CULL_FRONT;
		hr = this->device->CreateRasterizerState(&rs_cull_front_desc, this->rs_cull_front.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create rasterizer state.");

		//CREATE BLEND STATE
		D3D11_BLEND_DESC blendDesc = { 0 };
		D3D11_RENDER_TARGET_BLEND_DESC rtbd = { 0 };
		rtbd.BlendEnable = true;
		rtbd.SrcBlend = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
		rtbd.DestBlend = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
		rtbd.BlendOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;
		rtbd.SrcBlendAlpha = D3D11_BLEND::D3D11_BLEND_ONE;
		rtbd.DestBlendAlpha = D3D11_BLEND::D3D11_BLEND_ZERO;
		rtbd.BlendOpAlpha = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;
		rtbd.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE::D3D11_COLOR_WRITE_ENABLE_ALL;
		blendDesc.RenderTarget[0] = rtbd;
		hr = this->device->CreateBlendState(&blendDesc, this->blendState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create blend state.");

		//INIT TEXT AND FONT
		spriteBatch = std::make_unique<SpriteBatch>(this->deviceContext.Get());
		spriteFont = std::make_unique<SpriteFont>(this->device.Get(), L"Data/Fonts/consolas_16.spritefont");

		//CREATE SAMPLER STATE
		CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		hr = this->device->CreateSamplerState(&samplerDesc, this->samplerState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create sampler state.");
	}
	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}
	return true;
}

bool Graphics::InitializeShaders()
{
	std::wstring shaderFolder = L"";

#pragma region DetermineShaderPath
	if (IsDebuggerPresent() == TRUE)
	{
#ifdef _DEBUG //Debug Mode
#ifdef _WIN64 //x64
		shaderfolder = L"..\\x64\\Debug\\";
#else   //x86 (Win32)
		shaderFolder = L"..\\Debug\\";
#endif
#else
#ifdef _WIN64 //x64
		shaderfolder = L"..\\x64\\Release\\";
#else   //x86 (Win32)
		shaderFolder = L"..\\Release\\";
#endif
#endif
	}
#pragma endregion

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"TEXCOORD", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	UINT numElements = ARRAYSIZE(layout);

	if (!vertexShader.Initialize(this->device, shaderFolder + L"VertexShader.cso", layout, numElements))
		return false;

	if (!pixelShader.Initialize(this->device, shaderFolder + L"PixelShader.cso"))
		return false;

	return true;
}

bool Graphics::InitializeScene()
{
	try
	{
		//CREATE VERTEX AND INDEX BUFFERS
		Vertex rectangle[] =
		{
			//       x      y      z      u      v
		   Vertex(-0.5f,  0.5f, -0.5f,  0.0f,  0.0f), // Front Top Left		0
		   Vertex(0.5f,  0.5f, -0.5f,  1.0f,  0.0f), // Front Top Right		1
		   Vertex(-0.5f, -0.5f, -0.5f,  0.0f,  1.0f), // Front Bottom Left		2
		   Vertex(0.5f, -0.5f, -0.5f,  1.0f,  1.0f), // Front Bottom Right	3

		   Vertex(-0.5f,  0.5f,  0.5f,  0.0f,  0.0f), // Back Top Left			4
		   Vertex(0.5f,  0.5f,  0.5f,  1.0f,  0.0f), // Back Top Right		5
		   Vertex(-0.5f, -0.5f,  0.5f,  0.0f,  1.0f), // Back Bottom Left		6
		   Vertex(0.5f, -0.5f,  0.5f,  1.0f,  1.0f), // Back Bottom Right		7
		};

		DWORD indicies[] =
		{
			0, 1, 2, 2, 1, 3, //Front
			3, 1, 7, 7, 1, 5, //Right
			5, 4, 7, 7, 4, 6, //Back
			6, 4, 2, 2, 4, 0, //Left
			0, 4, 1, 1, 4, 5, //Top
			6, 2, 7, 7, 2, 3, //Back
		};


		HRESULT hr = this->vertexBuffer.Initialize(this->device.Get(), rectangle, ARRAYSIZE(rectangle));
		COM_ERROR_IF_FAILED(hr, "Failed to initalize vertex buffer.");

		hr = this->indexBuffer.Initialize(this->device.Get(), indicies, ARRAYSIZE(indicies));
		COM_ERROR_IF_FAILED(hr, "Failed to initalize index buffer.");


		//LOAD TEXTURES
		hr = CreateWICTextureFromFile(this->device.Get(), L"Data\\Textures\\seamless_grass.png", nullptr, seamless_grass.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create WIC texture from file.");


		hr = CreateWICTextureFromFile(this->device.Get(), L"Data\\Textures\\missing_texture.png", nullptr, pink_texture.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create WIC texture from file.");


		hr = CreateWICTextureFromFile(this->device.Get(), L"Data\\Textures\\ground_pavement_brick_01.png", nullptr, seamless_tile.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create WIC texture from file.");


		//INIT SHADERS
		hr = this->cb_vs_vertexShader.Initialize(this->device.Get(), this->deviceContext.Get());
		COM_ERROR_IF_FAILED(hr, "Failed to initialize vertex shader.");


		hr = this->cb_ps_pixelShader.Initialize(this->device.Get(), this->deviceContext.Get());
		COM_ERROR_IF_FAILED(hr, "Failed to initialize pixel shader.");


		camera.SetPosition(0.0f, 0.0f, -2.0f);
		camera.SetProjectionValues(90.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.1f, 1000.0f);
	}
	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}
	return true;
}

void Graphics::RenderFrame()
{
	float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	this->deviceContext->ClearRenderTargetView(this->renderTargetView.Get(), color);
	this->deviceContext->ClearDepthStencilView(this->depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	this->deviceContext->IASetInputLayout(this->vertexShader.GetInputLayout());
	this->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	this->deviceContext->RSSetState(rasterizerState.Get());

	this->deviceContext->VSSetShader(vertexShader.GetShader(), NULL, 0);
	this->deviceContext->PSSetSamplers(0, 1, this->samplerState.GetAddressOf());
	this->deviceContext->PSSetShader(pixelShader.GetShader(), NULL, 0);

	this->deviceContext->OMSetDepthStencilState(this->depthStencilState.Get(), 0);
	this->deviceContext->OMSetBlendState(blendState.Get(), NULL, 0xFFFFFFFF);

	UINT offset = 0;

	static float alpha = 0.5f;
	{ //Tile

		//Setup WPV matrix
		static float translationOffset[3] = { 0.0f, 0.0f, 4.0f };
		XMMATRIX world = XMMatrixScaling(5.0f, 5.0f, 5.0f) * XMMatrixTranslation(translationOffset[0], translationOffset[1], translationOffset[2]);

		//Update Constant Buffer
		this->cb_vs_vertexShader.data.mat = world * camera.GetViewMatrix() * camera.GetProjectionMatrix();
		this->cb_vs_vertexShader.data.mat = XMMatrixTranspose(cb_vs_vertexShader.data.mat);

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		if (!cb_vs_vertexShader.ApplyChanges())
			return;
		this->deviceContext->VSSetConstantBuffers(0, 1, cb_vs_vertexShader.GetAddressOf());

		this->cb_ps_pixelShader.data.alpha = alpha;
		this->cb_ps_pixelShader.ApplyChanges();
		this->deviceContext->PSSetConstantBuffers(0, 1, cb_ps_pixelShader.GetAddressOf());

		//square
		this->deviceContext->PSSetShaderResources(0, 1, this->seamless_tile.GetAddressOf());
		this->deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), vertexBuffer.StridePtr(), &offset);
		this->deviceContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		this->deviceContext->RSSetState(rs_cull_front.Get());
		this->deviceContext->DrawIndexed(indexBuffer.BufferSize(), 0, 0);
		this->deviceContext->RSSetState(rasterizerState.Get());
		this->deviceContext->DrawIndexed(indexBuffer.BufferSize(), 0, 0);
	}

	//{ //Grass

	//	//Setup WPV matrix
	//	static float translationOffset[3] = { 0.0f };
	//	XMMATRIX world = XMMatrixScaling(5.0f, 5.0f, 5.0f) * XMMatrixTranslation(translationOffset[0], translationOffset[1], translationOffset[2]);


	//	//Update Constant Buffer
	//	this->cb_vs_vertexShader.data.mat = world * camera.GetViewMatrix() * camera.GetProjectionMatrix();
	//	this->cb_vs_vertexShader.data.mat = XMMatrixTranspose(cb_vs_vertexShader.data.mat);

	//	D3D11_MAPPED_SUBRESOURCE mappedResource;
	//	if (!cb_vs_vertexShader.ApplyChanges())
	//		return;
	//	this->deviceContext->VSSetConstantBuffers(0, 1, cb_vs_vertexShader.GetAddressOf());

	//	this->cb_ps_pixelShader.data.alpha = 1.0f;
	//	this->cb_ps_pixelShader.ApplyChanges();
	//	this->deviceContext->PSSetConstantBuffers(0, 1, cb_ps_pixelShader.GetAddressOf());

	//	//square
	//	this->deviceContext->PSSetShaderResources(0, 1, this->seamless_grass.GetAddressOf());
	//	this->deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), vertexBuffer.StridePtr(), &offset);
	//	this->deviceContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	//	this->deviceContext->DrawIndexed(indexBuffer.BufferSize(), 0, 0);
	//}

	//{ //Pink Texture

	//	//Setup WPV matrix
	//	static float translationOffset[3] = { 0.0f, 0.0f, -1.0f };
	//	XMMATRIX world = XMMatrixTranslation(translationOffset[0], translationOffset[1], translationOffset[2]);

	//	//Update Constant Buffer
	//	this->cb_vs_vertexShader.data.mat = world * camera.GetViewMatrix() * camera.GetProjectionMatrix();
	//	this->cb_vs_vertexShader.data.mat = XMMatrixTranspose(cb_vs_vertexShader.data.mat);

	//	D3D11_MAPPED_SUBRESOURCE mappedResource;
	//	if (!cb_vs_vertexShader.ApplyChanges())
	//		return;
	//	this->deviceContext->VSSetConstantBuffers(0, 1, cb_vs_vertexShader.GetAddressOf());

	//	this->cb_ps_pixelShader.data.alpha = alpha;
	//	this->cb_ps_pixelShader.ApplyChanges();
	//	this->deviceContext->PSSetConstantBuffers(0, 1, cb_ps_pixelShader.GetAddressOf());

	//	//square
	//	this->deviceContext->PSSetShaderResources(0, 1, this->pink_texture.GetAddressOf());
	//	this->deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), vertexBuffer.StridePtr(), &offset);
	//	this->deviceContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	//	this->deviceContext->DrawIndexed(indexBuffer.BufferSize(), 0, 0);
	//}

	//Draw Text
	static int fpsCounter = 0;
	static std::string fpsString = "FPS: 0";
	fpsCounter += 1;
	if (fpsTimer.GetMillisecondsElapsed() >= 1000.0)
	{
		fpsString = "FPS: " + std::to_string(fpsCounter);
		fpsCounter = 0;
		fpsTimer.Restart();
	}
	spriteBatch->Begin();
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(fpsString).c_str(), XMFLOAT2(0.0f, 0.0f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f));
	spriteBatch->End();

#pragma region ImGui

	///////////////////////////////////////
	//////////////// ImGui ////////////////
	///////////////////////////////////////

	static int counter = 0;

	//Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//Create ImGui Test Window
	ImGui::Begin("Test");
	ImGui::Text("This is example text.");
	if (ImGui::Button("CLICK ME!"))
	{
		counter += 1;
	}
	ImGui::SameLine();
	std::string clickCount = "Click Count: " + std::to_string(counter);
	ImGui::Text(clickCount.c_str());

	ImGui::SliderFloat("Alpha", &alpha, 0.1f, 1.0f);

	ImGui::End();

	//Assemble draw data
	ImGui::Render();

	//Render draw data
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	///////////////////////////////////////
	///////////////////////////////////////
	///////////////////////////////////////

#pragma endregion

	this->swapchain->Present(0, NULL);
}