///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File changes (yyyy-mm-dd)
// 2016-09-07: filip.strugar@intel.com: first commit
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;

#pragma pack(push, 8) // Make sure we have consistent structure packings

struct ASSAO_Float4x4
{
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};

		float m[4][4];
		float arr[16];
	};

	ASSAO_Float4x4()
	{
		for (int i = 0; i < 16; i++)
			arr[i] = 0;
	}

	ASSAO_Float4x4(const float* inMatrix)
	{
		for (int i = 0; i < 16; i++)
			arr[i] = inMatrix[i];
	}

	inline void Transpose()
	{
		swap(arr[1], arr[4]);
		swap(arr[2], arr[8]);
		swap(arr[3], arr[12]);
		swap(arr[6], arr[9]);
		swap(arr[7], arr[13]);
		swap(arr[11], arr[14]);
	}

	inline void SetIdentity()
	{
		_11 = 1.0f;
		_12 = 0.0f;
		_13 = 0.0f;
		_14 = 0.0f;
		_21 = 0.0f;
		_22 = 1.0f;
		_23 = 0.0f;
		_24 = 0.0f;
		_31 = 0.0f;
		_32 = 0.0f;
		_33 = 1.0f;
		_34 = 0.0f;
		_41 = 0.0f;
		_42 = 0.0f;
		_43 = 0.0f;
		_44 = 1.0f;
	}

private:

	inline static void swap(float& a, float& b)
	{
		float temp = b;
		b = a;
		a = temp;
	}
};

// supported platforms - at the moment only DirectX11
enum class ASSAO_SupportedPlatforms
{
	InvalidValue,
	PC_DirectX11,
};

// effect visual settings
struct ASSAO_Settings
{
	float Radius; // [0.0,  ~ ] World (view) space size of the occlusion sphere.
	float ShadowMultiplier; // [0.0, 5.0] Effect strength linear multiplier
	float ShadowPower; // [0.5, 5.0] Effect strength pow modifier
	float ShadowClamp; // [0.0, 1.0] Effect max limit (applied after multiplier but before blur)
	float HorizonAngleThreshold; // [0.0, 0.2] Limits self-shadowing (makes the sampling area less of a hemisphere, more of a spherical cone, to avoid self-shadowing and various artifacts due to low tessellation and depth buffer imprecision, etc.)
	float FadeOutFrom; // [0.0,  ~ ] Distance to start start fading out the effect.
	float FadeOutTo; // [0.0,  ~ ] Distance at which the effect is faded out.
	int QualityLevel; // [  0,    ] Effect quality; 0 - low, 1 - medium, 2 - high, 3 - very high; each quality level is roughly 2x more costly than the previous, except the q3 which is variable but, in general, above q2.
	float Dummy2;
	int BlurPassCount; // [  0,   6] Number of edge-sensitive smart blur passes to apply. Quality 0 is an exception with only one 'dumb' blur pass used.
	float Sharpness; // [0.0, 1.0] (How much to bleed over edges; 1: not at all, 0.5: half-half; 0.0: completely ignore edges)
	float Dummy0;
	float Dummy1;
	float DetailShadowStrength; // [0.0, 5.0] Used for high-res detail AO using neighboring depth pixels: adds a lot of detail but also reduces temporal stability (adds aliasing).
	bool SkipHalfPixelsOnLowQualityLevel; // [true/false] Use half of the pixels (checkerboard pattern) when in Low quality for "Lowest" setting

	ASSAO_Settings();
};

struct ASSAO_CreateDesc
{
	ASSAO_SupportedPlatforms Platform;

protected:

	ASSAO_CreateDesc() { Platform = ASSAO_SupportedPlatforms::InvalidValue; }

public:

	virtual ~ASSAO_CreateDesc()
	{
	}
};

struct ASSAO_Inputs
{
	// Output scissor rect - used to draw AO effect to a sub-rectangle, for example, for performance reasons when using wider-than-screen depth input to avoid close-to-border artifacts.
	int ScissorLeft;
	int ScissorTop;
	int ScissorRight;
	int ScissorBottom;

	// Custom viewports not supported yet; this is here for future support. ViewportWidth and ViewportHeight must match or be smaller than source depth and normalmap sizes.
	int ViewportX;
	int ViewportY;
	int ViewportWidth;
	int ViewportHeight;

	// Requires a projection matrix created with xxxPerspectiveFovLH or equivalent, not tested (yet) for right-handed
	// coordinates and will likely break for ortho projections.
	ASSAO_Float4x4 ProjectionMatrix;

	bool DrawOpaque;

protected:

	ASSAO_Inputs()
	{
		ScissorLeft = 0;
		ScissorTop = 0;
		ScissorRight = 0;
		ScissorBottom = 0;
		ViewportX = 0;
		ViewportY = 0;
		ViewportWidth = 0;
		ViewportHeight = 0;
		DrawOpaque = false;
	}

public:

	virtual ~ASSAO_Inputs()
	{
	}
};

//
struct ASSAO_CreateDescDX11 : ASSAO_CreateDesc
{
	ID3D11Device* Device;
	const void* ShaderData;
	size_t ShaderDataSize;

	// data pointed to by shaderData is only used during a call to ASSAO_Effect::CreateInstance
	ASSAO_CreateDescDX11(ID3D11Device* device, const void* shaderData, size_t shaderDataSize) : Device(device), ShaderData(shaderData), ShaderDataSize(shaderDataSize) { Platform = ASSAO_SupportedPlatforms::PC_DirectX11; }
};

// 
struct ASSAO_InputsDX11 : ASSAO_Inputs
{
	ID3D11DeviceContext* DeviceContext;

	// Hardware screen depths
	//  - R32_FLOAT (R32_TYPELESS) or R24_UNORM_X8_TYPELESS (R24G8_TYPELESS) texture formats are supported.
	//  - Multisampling not yet supported.
	//  - Decoded using provided ProjectionMatrix.
	//  - For custom decoding see PSPrepareDepths/PSPrepareDepthsAndNormals where they get converted to linear viewspace storage.
	ID3D11ShaderResourceView* DepthSRV;

	// Viewspace normals (optional) 
	//  - If NULL, normals are generated from the depth buffer, otherwise provided normals are used for AO. ASSAO is less
	//    costly when input normals are provided, and has a more defined effect. However, aliasing in normals can result in 
	//    aliasing/flickering in the effect so, in some rare cases, normals generated from the depth buffer can look better.
	//  - _FLOAT or _UNORM texture formats are supported.
	//  - Input normals are expected to be in viewspace, encoded in [0, 1] with "encodedNormal.xyz = (normal.xyz * 0.5 + 0.5)" 
	//    or similar. Decode is done in LoadNormal() function with "normal.xyz = (encodedNormal.xyz * 2.0 - 1.0)", which can be
	//    easily modified for any custom decoding.
	//  - Use SSAO_SMOOTHEN_NORMALS for additional normal smoothing to reduce aliasing/flickering. This, however, also reduces
	//    high detail AO and increases cost.
	ID3D11ShaderResourceView* NormalSRV;

	ASSAO_InputsDX11()
	{
		DeviceContext = nullptr;
		DepthSRV = nullptr;
		NormalSRV = nullptr;
	}
};

#pragma pack( pop )

class ASSAO_Effect
{
protected:

	ASSAO_Effect()
	{
	};

	virtual ~ASSAO_Effect()
	{
	};

public:

	virtual void PreAllocateVideoMemory(const ASSAO_Inputs* inputs) = 0;
	virtual void DeleteAllocatedVideoMemory() = 0;

	// Returns currently allocated video memory in bytes; only valid after PreAllocateVideoMemory / Draw calls
	virtual unsigned int GetAllocatedVideoMemory() = 0;

	// Apply the SSAO effect to the currently selected render target using provided settings and platform-dependent inputs
	virtual void Draw(const ASSAO_Settings& settings, const ASSAO_Inputs* inputs) = 0;

public:

	static ASSAO_Effect* CreateInstance(const ASSAO_CreateDescDX11* createDesc);
	static void DestroyInstance(ASSAO_Effect* effectInstance);
};
