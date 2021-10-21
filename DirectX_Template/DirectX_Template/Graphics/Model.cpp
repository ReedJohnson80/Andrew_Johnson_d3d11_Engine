#include "Model.h"
bool Model::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* texture, ConstantBuffer<CB_VS_VertexShader>& cb_vs_vertexshader)
{
	this->device = device;
	this->deviceContext = deviceContext;
	this->texture = texture;
	this->cb_vs_vertexshader = &cb_vs_vertexshader;

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

		HRESULT hr = this->vertexBuffer.Initialize(this->device, rectangle, ARRAYSIZE(rectangle));
		COM_ERROR_IF_FAILED(hr, "Failed to initalize vertex buffer.");

		DWORD indicies[] =
		{
			0, 1, 2, 2, 1, 3, //Front
			3, 1, 7, 7, 1, 5, //Right
			5, 4, 7, 7, 4, 6, //Back
			6, 4, 2, 2, 4, 0, //Left
			0, 4, 1, 1, 4, 5, //Top
			6, 2, 7, 7, 2, 3, //Back
		};

		hr = this->indexBuffer.Initialize(this->device, indicies, ARRAYSIZE(indicies));
		COM_ERROR_IF_FAILED(hr, "Failed to initalize index buffer.");

	}
	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}

	this->UpdateWorldMatrix();
	return true;
}

void Model::SetTexture(ID3D11ShaderResourceView* texture)
{
	this->texture = texture;
}

void Model::Draw(const XMMATRIX& viewProjectionMatrix)
{
	//Update Constant Buffer with WPV Matrix
	this->cb_vs_vertexshader->data.mat = this->worldMatrix * viewProjectionMatrix;
	this->cb_vs_vertexshader->data.mat = XMMatrixTranspose(this->cb_vs_vertexshader->data.mat);
	this->cb_vs_vertexshader->ApplyChanges();
	this->deviceContext->VSSetConstantBuffers(0, 1, this->cb_vs_vertexshader->GetAddressOf());

	this->deviceContext->PSSetShaderResources(0, 1, &this->texture); //Set Texture
	this->deviceContext->IASetIndexBuffer(this->indexBuffer.Get(), DXGI_FORMAT::DXGI_FORMAT_R32_UINT, 0);
	UINT offset = 0;
	this->deviceContext->IASetVertexBuffers(0, 1, this->vertexBuffer.GetAddressOf(), this->vertexBuffer.StridePtr(), &offset);
	this->deviceContext->DrawIndexed(this->indexBuffer.BufferSize(), 0, 0);
}

void Model::UpdateWorldMatrix()
{
	this->worldMatrix = XMMatrixIdentity();
}
