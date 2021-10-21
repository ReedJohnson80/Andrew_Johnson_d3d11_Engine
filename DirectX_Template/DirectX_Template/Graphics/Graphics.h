#pragma once
#include "AdapterReader.h"
#include "Shaders.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <WICTextureLoader.h>
#include "Camera.h"
#include "..\\Timer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "Model.h"

class Graphics
{
public:
	bool Initialize(HWND hwnd, int width, int height);
	void RenderFrame();
	Camera												camera;

private:

//Funcs
	bool InitializeDirectX(HWND hwnd);
	bool InitializeShaders();
	bool InitializeScene();

//Vars
	int windowWidth = 0;
	int windowHeight = 0;
	Timer												fpsTimer;

//Device & Swapchain
	Microsoft::WRL::ComPtr<ID3D11Device>				device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>			deviceContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>				swapchain;

//Shaders
	VertexShader										vertexShader;
	PixelShader											pixelShader;

//Models
	Model												model;

//Buffers
	ConstantBuffer<CB_VS_VertexShader>					cb_vs_vertexShader;
	ConstantBuffer<CB_PS_PixelShader>					cb_ps_pixelShader;

//Views
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>		depthStencilView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>				depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState>		depthStencilState;

//States
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>		rasterizerState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>		rs_cull_front;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>			samplerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState>			blendState;

//Textures
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	pink_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	seamless_grass;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	seamless_tile;

	std::unique_ptr<DirectX::SpriteBatch>				spriteBatch;
	std::unique_ptr<DirectX::SpriteFont>				spriteFont;

};

