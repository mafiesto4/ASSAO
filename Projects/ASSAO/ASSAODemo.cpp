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

#include "ASSAODemo.h"

#include "Rendering/DirectX/vaRenderDeviceDX11.h"

#include <functional>

#include "Core/Misc/vaPoissonDiskGenerator.h"

using namespace VertexAsylum;

// TODO: (Leigh's comment): have a camera setting per scene so you don't appear in weird places when changing the camera
// TODO: (Leigh's comment): toggling between advanced and simple UI - should be one unique button in the same place
// TODO: (Leigh's comment): camera reset button

ASSAODemo::ASSAODemo( )
{
    //m_autoRunBenchmark      = false;
    //m_benchmarkOutPath      = L"";

    m_camera = std::shared_ptr<vaCameraBase>( new vaCameraBase() );

    m_camera->SetPosition( vaVector3( 4.3f, 29.2f, 14.2f ) );
    m_camera->SetOrientationLookAt( vaVector3( 6.5f, 0.0f, 8.7f ) );

    m_camera->SetNearPlane( 0.1f );
    m_camera->SetFarPlane( 10000.0f );
    m_camera->SetYFOV( 90.0f / 360.0f * VA_PIf );

    m_cameraFreeFlightController    = std::shared_ptr<vaCameraControllerFreeFlight>( new vaCameraControllerFreeFlight() );
    m_cameraFreeFlightController->SetMoveWhileNotCaptured( false );

    m_renderingGlobals  = VA_RENDERING_MODULE_CREATE_UNIQUE( vaRenderingGlobals );
    m_sky               = VA_RENDERING_MODULE_CREATE_UNIQUE( vaSky );
    m_sky->GetSettings().FogDistanceMin = 512.0f;

    m_GBuffer               = VA_RENDERING_MODULE_CREATE_UNIQUE( vaGBuffer );
    m_lighting              = VA_RENDERING_MODULE_CREATE_UNIQUE( vaLighting );
    m_postProcess           = VA_RENDERING_MODULE_CREATE_UNIQUE( vaPostProcess );
    m_postProcessTonemap    = VA_RENDERING_MODULE_CREATE_UNIQUE( vaPostProcessTonemap );

    m_SSAOEffect                    = VA_RENDERING_MODULE_CREATE_UNIQUE( ASSAOWrapper );

    m_loadedSceneChoice = ASSAODemo::SceneSelectionType::MaxCount;

    m_assetsDragon      = shared_ptr<vaAssetPack>( new vaAssetPack("Dragon") );
    m_assetsSibenik     = shared_ptr<vaAssetPack>( new vaAssetPack("Sibenik") );
    m_assetsSponza      = shared_ptr<vaAssetPack>( new vaAssetPack("Sponza") );
    m_assetsLostEmpire  = shared_ptr<vaAssetPack>( new vaAssetPack("LostEmpire") );

    {
        vaFileStream fileIn;
        if( fileIn.Open( vaCore::GetExecutableDirectory( ) + L"camerapos.data", FileCreationMode::Open ) )
        {
            m_camera->Load( fileIn );
        }
    }
    m_camera->AttachController( m_cameraFreeFlightController );

    {
        vaFileStream fileIn;
        if( fileIn.Open( vaCore::GetExecutableDirectory( ) + L"SSAODemoSettings.data", FileCreationMode::Open ) )
        {
            int32 size;
            if( fileIn.ReadValue<int32>(size) && (size == (int32)sizeof( m_settings )) )
            {
                fileIn.ReadValue( m_settings );
            }
        }
    }

    m_flythroughCameraEnabled = false;
}

ASSAODemo::~ASSAODemo( )
{ 
#if 1 || defined( _DEBUG )
    SaveCamera( );

    {
        vaFileStream fileOut;
        if( fileOut.Open( vaCore::GetExecutableDirectory( ) + L"SSAODemoSettings.data", FileCreationMode::Create ) )
        {
            fileOut.WriteValue<int32>( (int32)sizeof(m_settings) );
            fileOut.WriteValue( m_settings );
        }
    }
#endif

    // not needed but useful for debugging so I left it in
    m_meshDrawList.Reset();
    m_sceneMeshes.clear();
    m_sceneMeshesTransforms.clear();
}

void ASSAODemo::SetupScene( )
{
    m_sky->GetSettings().SunElevation   = VA_PIf * 0.25f;
    m_sky->GetSettings().SunAzimuth     = -1.0f;

    m_lighting->SetAmbientLightIntensity( vaVector3( 0.65f, 0.65f, 0.65f ) );
    m_lighting->SetDirectionalLightIntensity( vaVector3( 0.4f, 0.4f, 0.4f ) );

    const float keyTime = 10.0f;

    m_flythroughCameraControllerSibenik = std::shared_ptr<vaCameraControllerFocusLocationsFlythrough>( new vaCameraControllerFocusLocationsFlythrough( ) );
    {
        m_flythroughCameraControllerSibenik->SetFixedUp( true ); // so that we can seamlessly switch between flythrough and manual camera

        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 16.820f, 0.289f, 5.752f ), vaQuaternion( 0.591f, -0.564f, -0.398f, 0.417f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 17.244f, 2.268f, 9.419f ), vaQuaternion( 0.797f, -0.436f, -0.201f, 0.367f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 17.244f, 2.268f, 9.419f ), vaQuaternion( 0.392f, -0.275f, -0.504f, 0.719f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 14.309f, -1.641f, 3.127f ), vaQuaternion( -0.445f, 0.581f, 0.541f, -0.415f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 14.043f, 0.294f, 2.762f ), vaQuaternion( 0.524f, -0.505f, -0.476f, 0.494f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 6.251f, -0.121f, 4.470f ), vaQuaternion( 0.616f, -0.589f, -0.362f, 0.379f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -4.851f, -5.866f, 1.764f ), vaQuaternion( -0.325f, 0.566f, 0.657f, -0.378f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -12.964f, -6.167f, 2.231f ), vaQuaternion( -0.053f, 0.735f, 0.674f, -0.048f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -16.166f, -4.147f, 2.707f ), vaQuaternion( 0.801f, -0.446f, -0.194f, 0.349f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -18.692f, 2.723f, 1.157f ), vaQuaternion( 0.513f, 0.343f, 0.438f, 0.654f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -15.549f, 2.016f, 15.341f ), vaQuaternion( 0.681f, 0.525f, 0.312f, 0.404f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 0.511f, -0.125f, 14.342f ), vaQuaternion( 0.643f, 0.655f, 0.283f, 0.278f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 8.394f, 4.057f, 8.990f ), vaQuaternion( 0.897f, 0.165f, 0.074f, 0.402f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 8.923f, 7.392f, 4.358f ), vaQuaternion( 0.667f, -0.005f, -0.006f, 0.745f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 13.683f, 5.797f, 8.309f ), vaQuaternion( 0.753f, -0.361f, -0.237f, 0.496f ), keyTime ) );
        m_flythroughCameraControllerSibenik->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 15.177f, 0.169f, 8.228f ), vaQuaternion( 0.490f, -0.476f, -0.508f, 0.524f ), keyTime ) );
    }

    m_flythroughCameraControllerSponza = std::shared_ptr<vaCameraControllerFocusLocationsFlythrough>( new vaCameraControllerFocusLocationsFlythrough( ) );
    {
        m_flythroughCameraControllerSponza->SetFixedUp( true ); // so that we can seamlessly switch between flythrough and manual camera

        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 9.142f, -0.315f, 3.539f ), vaQuaternion( 0.555f, 0.552f, 0.439f, 0.441f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 11.782f, -0.078f, 1.812f ), vaQuaternion( 0.463f, -0.433f, -0.528f, 0.565f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 5.727f, -1.077f, 2.716f ), vaQuaternion( -0.336f, 0.619f, 0.624f, -0.339f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -2.873f, 1.043f, 2.808f ), vaQuaternion( 0.610f, -0.378f, -0.367f, 0.592f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -7.287f, 1.254f, 2.598f ), vaQuaternion( 0.757f, 0.004f, 0.003f, 0.654f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -12.750f, 0.051f, 2.281f ), vaQuaternion( 0.543f, 0.448f, 0.452f, 0.548f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -14.431f, -3.854f, 2.411f ), vaQuaternion( 0.556f, 0.513f, 0.443f, 0.481f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -14.471f, -6.127f, 1.534f ), vaQuaternion( 0.422f, 0.520f, 0.577f, 0.467f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -8.438f, -5.876f, 4.094f ), vaQuaternion( 0.391f, 0.784f, 0.432f, 0.215f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -2.776f, -4.915f, 1.890f ), vaQuaternion( 0.567f, 0.646f, 0.384f, 0.337f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -1.885f, -4.796f, 2.499f ), vaQuaternion( 0.465f, 0.536f, 0.532f, 0.462f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 1.569f, -4.599f, 3.303f ), vaQuaternion( 0.700f, 0.706f, 0.079f, 0.078f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 4.799f, -5.682f, 3.353f ), vaQuaternion( 0.037f, 0.900f, 0.434f, 0.018f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 7.943f, -5.405f, 3.416f ), vaQuaternion( -0.107f, 0.670f, 0.725f, -0.115f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 11.445f, -3.276f, 3.319f ), vaQuaternion( -0.455f, 0.589f, 0.529f, -0.409f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 12.942f, 2.277f, 3.367f ), vaQuaternion( 0.576f, -0.523f, -0.423f, 0.465f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 12.662f, 3.895f, 4.186f ), vaQuaternion( 0.569f, -0.533f, -0.428f, 0.457f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 8.688f, 4.170f, 4.107f ), vaQuaternion( 0.635f, -0.367f, -0.340f, 0.588f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 6.975f, 1.525f, 4.299f ), vaQuaternion( 0.552f, -0.298f, -0.369f, 0.685f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 5.497f, -0.418f, 7.013f ), vaQuaternion( 0.870f, -0.124f, -0.067f, 0.473f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 9.520f, -2.108f, 6.619f ), vaQuaternion( 0.342f, 0.599f, 0.629f, 0.359f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( 11.174f, 3.226f, 6.969f ), vaQuaternion( -0.439f, 0.536f, 0.558f, -0.457f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -2.807f, 5.621f, 7.026f ), vaQuaternion( 0.694f, 0.013f, 0.014f, 0.720f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -11.914f, 5.271f, 7.026f ), vaQuaternion( 0.694f, 0.013f, 0.014f, 0.720f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -12.168f, 1.401f, 7.235f ), vaQuaternion( 0.692f, -0.010f, -0.011f, 0.722f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -6.541f, 0.038f, 7.491f ), vaQuaternion( 0.250f, -0.287f, -0.697f, 0.608f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -6.741f, 0.257f, 2.224f ), vaQuaternion( 0.511f, -0.465f, -0.487f, 0.535f ), keyTime ) );
        m_flythroughCameraControllerSponza->AddKey( vaCameraControllerFocusLocationsFlythrough::Keyframe( vaVector3( -10.913f, -0.020f, 2.766f ), vaQuaternion( 0.511f, -0.471f, -0.487f, 0.529f ), keyTime ) );
    }

    m_flythroughCameraControllerLostEmpire = std::shared_ptr<vaCameraControllerFocusLocationsFlythrough>( new vaCameraControllerFocusLocationsFlythrough( ) );
    {
        m_flythroughCameraControllerLostEmpire ->SetFixedUp( true ); // so that we can seamlessly switch between flythrough and manual camera
    }
}

std::shared_ptr<vaCameraControllerBase> ASSAODemo::GetCameraController( )
{
    if( m_flythroughCameraEnabled )
    {
        switch( m_settings.SceneChoice )
        {
        case (ASSAODemo::SceneSelectionType::Sibenik):
        case (ASSAODemo::SceneSelectionType::SibenikAndDragons):
            return m_flythroughCameraControllerSibenik;
        case (ASSAODemo::SceneSelectionType::Sponza):
        case (ASSAODemo::SceneSelectionType::SponzaAndDragons):
            return m_flythroughCameraControllerSponza;
        case (ASSAODemo::SceneSelectionType::LostEmpire):
            return m_flythroughCameraControllerLostEmpire;
        default:
            return m_cameraFreeFlightController;
            break;
        }
    }
    else
    {
        return m_cameraFreeFlightController;
    }
}

static wstring CameraFileName( int index )
{
    wstring fileName = vaCore::GetExecutableDirectory( ) + L"camerapos";
    if( index != -1 ) 
        fileName += vaStringTools::Format( L"_%d", index );
    fileName += L".data";
    return fileName;
}

void ASSAODemo::LoadCamera( int index )
{
    vaFileStream fileIn;
    if( fileIn.Open( CameraFileName(index), FileCreationMode::Open ) )
    {
        m_camera->Load( fileIn );
        m_camera->AttachController( m_cameraFreeFlightController );
    }
}

void ASSAODemo::SaveCamera( int index )
{
    vaFileStream fileOut;
    if( fileOut.Open( CameraFileName(index), FileCreationMode::Create ) )
    {
        m_camera->Save( fileOut );
    }
}

void ASSAODemo::EnsureLoaded( vaAssetPack & pack )
{
    if( pack.Count( ) == 0 )
    {
        wstring assetsPath = vaCore::GetExecutableDirectory( ) + L"Media/" + vaStringTools::SimpleWiden( pack.Name() ) + L".apack";

        vaSimpleScopeTimerLog timerLog( vaStringTools::Format( L"Loading '%s'", assetsPath.c_str( ) ) );
        vaFileStream fileIn;
        if( fileIn.Open( assetsPath, FileCreationMode::Open ) )
        {
            if( pack.Load( fileIn ) )
            {
                vaLog::GetInstance( ).Add( LOG_COLORS_SUCCESS, L"   loaded ok, %d assets.", (int)pack.Count() );
            }
            else
            {
                vaLog::GetInstance( ).Add( LOG_COLORS_ERROR, L"   error loading." );
            }
        }
        else
        {
            vaLog::GetInstance( ).Add( LOG_COLORS_ERROR, L"   error loading, asset file not found." );
        }
    }
}

void ASSAODemo::OnStarted( )
{
    SetupScene( );

    // load required assets
    EnsureLoaded( *m_assetsDragon );
}

void ASSAODemo::OnBeforeStopped( )
{
}

void ASSAODemo::OnStopped( )
{
}

void ASSAODemo::OnWndProcOverride( bool & overrideDefault, LRESULT & ret, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    // if already overridden 
    if( overrideDefault )
        return;
}

void ASSAODemo::OnResized( int width, int height, bool windowed )
{
    vaViewport mainViewport = m_renderDevice->GetMainContext( )->GetViewport();
    assert( mainViewport.Width == width && mainViewport.Height == height );

    m_camera->SetViewportSize( width, height );
}

void ASSAODemo::InsertAllToSceneMeshesList( vaAssetPack & pack, const vaMatrix4x4 & transform )
{
    for( size_t i = 0; i < pack.Count(); i++ )
    {
        auto asset = pack[ i ];
        if( asset->GetMesh( ) != nullptr )
        {
            m_sceneMeshes.push_back( *asset->GetMesh( ) );
            m_sceneMeshesTransforms.push_back( transform );
        }
    }
}

void ASSAODemo::OnTick( float deltaTime )
{
    if( deltaTime > 0.2f )
        deltaTime = 0.2f;

    {
        auto wantedCameraController = GetCameraController();
        if( m_camera->GetAttachedController( ) != wantedCameraController )
            m_camera->AttachController( wantedCameraController );
    }

    {
        const float minValidDelta = 0.0005f;
        if( deltaTime < minValidDelta )
        {
            m_renderDevice->GetCanvas2D( )->DrawString( 300, 40, 0xFFFF2020, 0xFF000000, "WARNING, delta time too small, clamping" );
        }
        deltaTime = vaMath::Max( deltaTime, minValidDelta ); // not correct above 1000 fps!
    }

    if( m_loadedSceneChoice != m_settings.SceneChoice )
    {
        m_loadedSceneChoice = m_settings.SceneChoice;

        m_sceneMeshes.clear();
        m_sceneMeshesTransforms.clear();

        m_sceneMeshes.push_back( vaRenderMesh::CreatePlane( vaMatrix4x4::Translation( 0.0f, 0.0f, -0.1f ), 1000.0f, 1000.0f ) );
        m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );

        switch( m_loadedSceneChoice )
        {
        case VertexAsylum::ASSAODemo::SceneSelectionType::Sponza:
        {
            EnsureLoaded( *m_assetsSponza );
            InsertAllToSceneMeshesList( *m_assetsSponza );
        }
        break;

        case VertexAsylum::ASSAODemo::SceneSelectionType::SponzaAndDragons:
        {
            EnsureLoaded( *m_assetsSponza );
            InsertAllToSceneMeshesList( *m_assetsSponza );

            m_sceneMeshes.push_back( vaRenderMesh::CreateTetrahedron(    vaMatrix4x4::Translation( -4.0f, 4.0f, 1.0f + 0.35f ), false ) );                                      m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );
            //m_sceneMeshes.push_back( vaRenderMesh::CreateCube(           vaMatrix4x4::Translation( -4.0f, 4.0f, 1.0f + 0.7f ), false ) );                                       m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );
            //m_sceneMeshes.push_back( vaRenderMesh::CreateOctahedron(     vaMatrix4x4::Translation( -2.0f, 4.0f, 1.0f + 1.0f ), false ) );                                       m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );
            //m_sceneMeshes.push_back( vaRenderMesh::CreateIcosahedron(    vaMatrix4x4::Translation(  0.0f, 4.0f, 1.0f + 0.85f ), false ) );                                      m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );
            //m_sceneMeshes.push_back( vaRenderMesh::CreateDodecahedron(   vaMatrix4x4::Translation(  2.0f, 4.0f, 1.0f + 0.94f ), false ) );                                      m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );
            //m_sceneMeshes.push_back( vaRenderMesh::CreateSphere(         vaMatrix4x4::Translation(  4.0f, 4.0f, 1.0f + 1.0f ), 1, false ) );                                    m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );
            //m_sceneMeshes.push_back( vaRenderMesh::CreateCylinder(       vaMatrix4x4::Translation(  6.0f, 4.0f, 1.0f + 1.02f ), 2.0f, 0.5f, 0.4f, 10, false, false ) );         m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );
            m_sceneMeshes.push_back( vaRenderMesh::CreateTeapot(         vaMatrix4x4::Scaling( 0.5f, 0.5f, 0.5f ) * vaMatrix4x4::Translation(  4.0f, 4.0f, 1.0f + 0.5f ) ) );  m_sceneMeshesTransforms.push_back( vaMatrix4x4::Identity );

            //InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 3.0f, 3.0f, 3.0f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation( -5.0f,  -4.7f, 1.8f ) );
            //InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 2.5f, 2.5f, 2.5f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation( -4.0f,  -4.5f, 1.65f ) );
            //InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 2.0f, 2.0f, 2.0f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation( -2.0f,  -4.5f, 1.5f ) );
            //InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 1.5f, 1.5f, 1.5f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation( -0.0f,  -4.5f, 1.38f ) );
            InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 1.5f, 1.5f, 1.5f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation(  -4.0f,  -5.3f, 1.38f ) );
            //InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 0.7f, 0.7f, 0.7f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation(  4.0f,  -4.5f, 1.19f ) );
            InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 0.5f, 0.5f, 0.5f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation(  4.0f,  -5.3f, 1.11f ) );
        }
        break;

        case VertexAsylum::ASSAODemo::SceneSelectionType::Sibenik:
        {
            EnsureLoaded( *m_assetsSibenik );
            InsertAllToSceneMeshesList( *m_assetsSibenik );
        } break;

        case VertexAsylum::ASSAODemo::SceneSelectionType::SibenikAndDragons:
        {
            EnsureLoaded( *m_assetsSibenik );
            InsertAllToSceneMeshesList( *m_assetsSibenik );
            InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 4.0f, 4.0f, 4.0f ) *  vaMatrix4x4::RotationZ( VA_PIf * 0.5f ) * vaMatrix4x4::Translation(  -14.0f,  0.0f, 1.2f ) );
            InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 3.0f, 3.0f, 3.0f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation(  -17.6f, -5.4f, 1.08f ) );
            //InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 3.0f, 3.0f, 3.0f ) *  vaMatrix4x4::RotationZ( -VA_PIf * 0.0f ) * vaMatrix4x4::Translation( -17.6f,  5.0f, 1.08f ) );
            //InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 2.0f, 2.0f, 2.0f ) *  vaMatrix4x4::RotationZ( VA_PIf * 1.0f ) * vaMatrix4x4::Translation(  -14.0f, -4.5f, 0.75f ) );
            InsertAllToSceneMeshesList( *m_assetsDragon, vaMatrix4x4::Scaling( 2.0f, 2.0f, 2.0f ) *  vaMatrix4x4::RotationZ( -VA_PIf * 0.0f ) * vaMatrix4x4::Translation( -14.0f,  5.5f, 0.75f ) );
        } break;

        case VertexAsylum::ASSAODemo::SceneSelectionType::LostEmpire:
        {
            EnsureLoaded( *m_assetsLostEmpire );
            InsertAllToSceneMeshesList( *m_assetsLostEmpire );
        } break;

        default:
            break;
        }
    }

    m_renderingGlobals->Tick( deltaTime );

    m_sky->Tick( deltaTime, m_lighting.get() );
    m_camera->Tick( deltaTime, m_application->HasFocus( ) );


    // calculate shadow map
    if( m_simpleShadowMap != nullptr )
    {
        const float sceneRadius = 50.0f;
        vaBoundingSphere shadowMapArea( vaVector3( 0.0f, 0.0f, 10.0f ), sceneRadius );
        vaVector3 lightDir = -m_sky->GetSunDir( );
        vaVector3 upRefDir = vaVector3( 0.0f, 0.0f, 1.0f );
        if( vaMath::Abs( vaVector3::Dot( lightDir, upRefDir ) ) > 0.95f )
            upRefDir = vaVector3( 0.0f, 1.0f, 0.0f );
        if( vaMath::Abs( vaVector3::Dot( lightDir, upRefDir ) ) > 0.95f )
            upRefDir = vaVector3( 1.0f, 0.0f, 0.0f );

        vaVector3 lightAxisZ = lightDir;
        vaVector3 lightAxisX = vaVector3::Cross( upRefDir, lightDir  ).Normalize();
        vaVector3 lightAxisY = vaVector3::Cross( lightDir, lightAxisX ).Normalize();

        vaMatrix3x3 shadowAxis( lightAxisX, lightAxisY, lightAxisZ );

        vaOrientedBoundingBox obb( shadowMapArea.Center, vaVector3( shadowMapArea.Radius, shadowMapArea.Radius, shadowMapArea.Radius * 2 ), shadowAxis );

        m_simpleShadowMap->UpdateArea( obb );

        if( m_settings.ShowWireframe )
        {
            vaDebugCanvas3DBase * canvas3D = GetCanvas3D( );
            canvas3D->DrawAxis( shadowMapArea.Center, shadowMapArea.Radius, &vaMatrix4x4( shadowAxis ) );
            canvas3D->DrawBox( obb, 0xFF00FF00, 0x10FFFFFF );
        }
    }

    // old data still queued? that's a bug!
    assert( m_meshDrawList.Count() == 0 );

    for( size_t i = 0; i < m_sceneMeshes.size( ); i++ )
    {
        vaMatrix4x4 transform = (m_sceneMeshesTransforms.size()>i)?(m_sceneMeshesTransforms[i]):(vaMatrix4x4::Identity);
        m_meshDrawList.Insert( m_sceneMeshes[i], transform ) ; //vaMatrix4x4::Translation( 0.0f, ( i - m_sceneMeshes.size( ) / 2.0f ) * 2.0f, ( i == 0 ) ? ( 0.0f ) : ( 1.0f ) ), 0, vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
    }

    if( m_application->HasFocus( ) && !vaInputMouseBase::GetCurrent( )->IsCaptured( ) )
    {
        static float notificationStopTimeout = 0.0f;
        notificationStopTimeout += deltaTime;

        vaInputKeyboardBase & keyboard = *vaInputKeyboardBase::GetCurrent( );
        if( keyboard.IsKeyDown( vaKeyboardKeys::KK_LEFT ) || keyboard.IsKeyDown( vaKeyboardKeys::KK_RIGHT ) || keyboard.IsKeyDown( vaKeyboardKeys::KK_UP ) || keyboard.IsKeyDown( vaKeyboardKeys::KK_DOWN ) ||
            keyboard.IsKeyDown( ( vaKeyboardKeys )'W' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'S' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'A' ) ||
            keyboard.IsKeyDown( ( vaKeyboardKeys )'D' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'Q' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'E' ) )
        {
            if( notificationStopTimeout > 3.0f )
            {
                notificationStopTimeout = 0.0f;
                vaLog::GetInstance().Add( vaVector4( 1.0f, 0.0f, 0.0f, 1.0f ), L"To switch into free flight (move&rotate) mode, use mouse right click." );
            }
        }
    }

    if( m_settings.UseSimpleUI )
    {
        m_settings.CameraYFov               = 90.0f / 360.0f * VA_PIf;
    }
}


static const ImVec4 colIntelBlue( 0.0f, 113.0f / 255.0f, 197.0f / 255.0f, 1.0f );
static const ImVec4 colIntelBlueFrameBk( 94.0f / 255.0f, 100.0f / 255.0f, 105.0f / 255.0f, 1.0f );
static const ImVec4 colIntelBlueBk( 0.0f, 60.0f / 255.0f, 113.0f / 255.0f, 1.0f );
static const ImVec4 colIntelBlueButton( 0.0f, 174.0f / 255.0f, 239.0f / 255.0f, 1.0f );
static const ImVec4 colIntelBlueButtonHovered( 0.0f, 186.0f / 255.0f, 245.0f / 255.0f, 1.0f );
static const ImVec4 colIntelBlueButtonActive( 0.0f, 131.0f / 255.0f, 215.0f / 255.0f, 1.0f );
static const ImVec4 colIntelText( 243.0f / 255.0f, 243.0f / 255.0f, 243.0f / 255.0f, 1.0f );
static const ImVec4 colIntelBlueButtonDisabled( 0.0f, 131.0f / 255.0f, 215.0f / 255.0f, 1.0f );
static const ImVec4 colIntelBlueButtonDisabledHovered( 0.0f, 140.0f / 255.0f, 220.0f / 255.0f, 1.0f );

static int SetImIntelColours( )
{
    int counter = 0;

    counter++; ImGui::PushStyleColor( ImGuiCol_ChildWindowBg, ImVec4( colIntelBlueBk.x, colIntelBlueBk.y, colIntelBlueBk.z, 0.95f ) );
    counter++; ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( colIntelBlueBk.x, colIntelBlueBk.y, colIntelBlueBk.z, 0.95f ) );
    counter++; ImGui::PushStyleColor( ImGuiCol_FrameBg, colIntelBlueFrameBk );
    counter++; ImGui::PushStyleColor( ImGuiCol_Text, colIntelText );
    counter++; ImGui::PushStyleColor( ImGuiCol_Button, colIntelBlueButton );
    counter++; ImGui::PushStyleColor( ImGuiCol_ButtonActive, colIntelBlueButtonActive );
    counter++; ImGui::PushStyleColor( ImGuiCol_ButtonHovered, colIntelBlueButtonHovered );
    counter++; ImGui::PushStyleColor( ImGuiCol_ScrollbarBg, colIntelBlueBk );
    counter++; ImGui::PushStyleColor( ImGuiCol_ScrollbarGrab, colIntelBlueButton );
    counter++; ImGui::PushStyleColor( ImGuiCol_ScrollbarGrabActive, colIntelBlueButtonActive );
    counter++; ImGui::PushStyleColor( ImGuiCol_ScrollbarGrabHovered, colIntelBlueButtonHovered );
    counter++; ImGui::PushStyleColor( ImGuiCol_SliderGrab, colIntelBlueButton );
    counter++; ImGui::PushStyleColor( ImGuiCol_SliderGrabActive, colIntelBlueButtonActive );
    counter++; ImGui::PushStyleColor( ImGuiCol_CheckMark, colIntelBlueButtonActive );

    return counter;
}

bool ImGuiToggleButton( const char* label, const ImVec2& size_arg, bool & value )
{
    vaVector4 buttonEnabled          = VAFromIm( ImGui::GetStyle().Colors[ ImGuiCol_Button ] );
    vaVector4 buttonDisabled         = vaVector4( 0.4f, 0.4f, 0.4f, 1.0f );

    vaVector4 buttonEnabledHovered   = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) - (vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) - buttonEnabled) * 0.9f;
    vaVector4 buttonDisabledHovered  = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) - (vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) - buttonDisabled) * 0.9f;

    if( value )
    {
        ImGui::PushStyleColor( ImGuiCol_Button,         ImFromVA( buttonEnabled ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,   ImFromVA( buttonEnabled ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered,  ImFromVA( buttonEnabledHovered ) );
        ImGui::PushStyleColor( ImGuiCol_Text, colIntelText );
    }
    else
    {
        ImGui::PushStyleColor( ImGuiCol_Button,         ImFromVA( buttonDisabled ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,   ImFromVA( buttonDisabled ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered,  ImFromVA( buttonDisabledHovered ) );
        ImGui::PushStyleColor( ImGuiCol_Text,           colIntelText );
    }

    bool changed = false;
    if( ImGui::ButtonEx( label, size_arg, ImGuiButtonFlags_PressedOnClick ) )
    {
        changed = true;
        value = !value;
    }

    ImGui::PopStyleColor( 4 );

    return changed;
}

void ASSAODemo::OnRender( )
{
    m_frameIndex++;

    vaRenderDeviceContext & mainContext = *m_renderDevice->GetMainContext( );

//// just for testing image comparison
//    {
//        vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ) );
//
//        shared_ptr<vaTexture> a0 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"a0.png", false, true ) );
//        shared_ptr<vaTexture> a1 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"a1.png", false, true ) );
//        shared_ptr<vaTexture> b0 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"b0.png", false, true ) );
//        shared_ptr<vaTexture> b1 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"b1.png", false, true ) );
//        shared_ptr<vaTexture> c0 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"c0.png", false, true ) );
//        shared_ptr<vaTexture> c1 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"c1.png", false, true ) );
//
//        vaVector4 diff;
//        VA_LOG( "  -------------------------------  ");
//        //diff = m_postProcess->CompareImages( drawContext, *a0.get(), *a0.get() );
//        //VA_LOG( " a0 vs a0: %.8f", diff.x );
//        //diff = m_postProcess->CompareImages( drawContext, *a0.get(), *a1.get() );
//        //VA_LOG( " a0 vs a1: %.8f", diff.x );
//        //diff = m_postProcess->CompareImages( drawContext, *a0.get(), *b0.get() );
//        //VA_LOG( " a0 vs b0: %.8f, PSNR: %.3f", diff.x, diff.y );
//        //diff = m_postProcess->CompareImages( drawContext, *a1.get(), *b1.get() );
//        //VA_LOG( " a1 vs b1: %.8f", diff.x );
//        //diff = m_postProcess->CompareImages( drawContext, *c0.get(), *c1.get() );
//        //VA_LOG( " c0 vs c1: %.8f", diff.x );
//
//        shared_ptr<vaTexture> z0 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"a0.png", false, true ) );
//        shared_ptr<vaTexture> z1 = shared_ptr<vaTexture>( vaTexture::Import( vaCore::GetWorkingDirectory( ) + L"b0.png", false, true ) );
//        diff = m_postProcess->CompareImages( drawContext, *z0.get(), *z1.get() );
//        VA_LOG( " a0 vs b0: %.8f, PSNR: %.3f", diff.x, diff.y );
//
//
//        VA_LOG( "  -------------------------------  ");
//        ::ExitProcess( 0 );
//    }


    vaViewport mainViewportBackup   = m_renderDevice->GetMainContext( )->GetViewport();
    vaViewport mainViewport         = m_renderDevice->GetMainContext( )->GetViewport();
    
    // current textures for sibenik are not good / complete
    vaRenderMaterialManager::GetInstance().SetTexturingDisabled( m_settings.DisableTexturing );
    if( m_settings.SceneChoice == ASSAODemo::SceneSelectionType::SibenikAndDragons )
    {
        vaRenderMaterialManager::GetInstance().SetTexturingDisabled( true );
    }

    vaVector4i scissorRectForSSAO( 0, 0, 0, 0 );

    // update resolution and camera FOV
    {
        double yScaleDueToBorder = (mainViewport.Height * 0.5) / (double)(mainViewport.Height * 0.5);

        double nonExpandedTan = std::tan( m_settings.CameraYFov / 2.0 );
        float expandedFOV = (float)(std::atan( nonExpandedTan * yScaleDueToBorder ) * 2.0);

        m_camera->SetYFOV( expandedFOV );
        m_camera->SetViewportSize(mainViewport.Width, mainViewport.Height);
        m_camera->Tick( 0.0f, false );  // re-tick for expanded focus

        scissorRectForSSAO.x   = 0;
        scissorRectForSSAO.y   = 0;
        scissorRectForSSAO.z   = mainViewport.Width;
        scissorRectForSSAO.w   = mainViewport.Height;
    }
 
    // update GBuffer resources if needed
    {
        vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ), m_lighting.get( ) );
        m_GBuffer->UpdateResources( drawContext, mainViewport.Width, mainViewport.Height );
    }

    // decide on the main render target / depth
    shared_ptr<vaTexture> mainColorRT   = m_GBuffer->GetOutputColor();  // m_renderDevice->GetMainChainColor();
    shared_ptr<vaTexture> mainDepthRT   = m_GBuffer->GetDepthBuffer();  // m_renderDevice->GetMainChainDepth();

    // clear the main render target / depth
    mainColorRT->ClearRTV( mainContext, vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
    mainDepthRT->ClearDSV( mainContext, true, m_camera->GetUseReversedZ()?(0.0f):(1.0f), false, 0 );

    // set main render target / depth
    mainContext.SetRenderTarget( mainColorRT, mainDepthRT, true );

    {
        VA_SCOPE_CPUGPU_TIMER( Deferred, mainContext );
        vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ), m_lighting.get( ) );

        // this sets up global constants
        m_renderingGlobals->SetAPIGlobals( drawContext );

        // clear light accumulation (radiance) RT
        m_GBuffer->GetRadiance()->ClearRTV( mainContext, vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );

        // Draw deferred elements into the GBuffer
        {
            VA_SCOPE_CPUGPU_TIMER( GBufferDraw, mainContext );
            drawContext.PassType = vaRenderPassType::Deferred;

            // GBuffer textures
            const std::shared_ptr<vaTexture> renderTargets[] = { m_GBuffer->GetAlbedo(), m_GBuffer->GetNormalMap() };

            // clear GBuffer
            for( int i = 0; i < _countof( renderTargets ); i++ )
                renderTargets[i]->ClearRTV( mainContext, vaVector4(0.0f, 0.0f, 0.0f, 0.0f) );

            mainContext.SetRenderTargets( _countof( renderTargets ), renderTargets, mainDepthRT, true );

            vaRenderMeshManager::GetInstance( ).Draw( drawContext, m_meshDrawList );
        }

        // GBuffer processing
        {
            VA_SCOPE_CPUGPU_TIMER( GBufferProcess, mainContext );
            vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ), m_lighting.get( ) );

            // this sets up global constants
            m_renderingGlobals->SetAPIGlobals( drawContext );

            // set destination render target and no depth
            mainContext.SetRenderTarget( m_GBuffer->GetDepthBufferViewspaceLinear( ), nullptr, true );

            m_GBuffer->DepthToViewspaceLinear( drawContext, *mainDepthRT.get( ) );
        }

        // Apply lighting
        {
            VA_SCOPE_CPUGPU_TIMER( Lighting, mainContext );
            drawContext.PassType = vaRenderPassType::Unknown;

            if( m_simpleShadowMap != nullptr )
            {
                m_simpleShadowMap->StartUsing( drawContext );

                if( m_simpleShadowMap->GetVolumeShadowMapPlugin( ) != NULL )
                    m_simpleShadowMap->GetVolumeShadowMapPlugin( )->StartUsing( drawContext, m_simpleShadowMap.get( ) );
            }

            // this sets up global constants
            m_renderingGlobals->SetAPIGlobals( drawContext );

            // set destination render target and no depth
            mainContext.SetRenderTarget( m_GBuffer->GetRadiance( ), nullptr, true );

            m_lighting->ApplyDirectionalAmbientLighting( drawContext, *m_GBuffer.get() );

            if( m_simpleShadowMap != nullptr )
            {
                if( m_simpleShadowMap->GetVolumeShadowMapPlugin( ) != NULL )
                    m_simpleShadowMap->GetVolumeShadowMapPlugin( )->StopUsing( drawContext, m_simpleShadowMap.get( ) );
                m_simpleShadowMap->StopUsing( drawContext );
            }
        }

        // restore main render target / depth
        mainContext.SetRenderTarget( mainColorRT, mainDepthRT, true );

        // Tonemap to final color
        {
            VA_SCOPE_CPUGPU_TIMER( Lighting, mainContext );
            drawContext.PassType = vaRenderPassType::Unknown;

            // this sets up global constants
            m_renderingGlobals->SetAPIGlobals( drawContext );

            m_postProcessTonemap->Tonemap( drawContext, *m_GBuffer->GetRadiance() );
        }
    }

    // Forward draw
    {
        VA_SCOPE_CPUGPU_TIMER( ForwardDraw, mainContext );
        vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ), m_lighting.get( ) );

        if( m_simpleShadowMap != nullptr )
        {
            m_simpleShadowMap->StartUsing( drawContext );

            if( m_simpleShadowMap->GetVolumeShadowMapPlugin() != NULL )
                m_simpleShadowMap->GetVolumeShadowMapPlugin()->StartUsing( drawContext, m_simpleShadowMap.get() );
        }

        // Draw opaque stuff
        {
            VA_SCOPE_CPUGPU_TIMER( Opaque, mainContext );

            //vaDrawContext drawContext( *m_camera.get( ), mainContext );
            drawContext.PassType          = vaRenderPassType::ForwardOpaque;

            // this sets up global constants
            m_renderingGlobals->SetAPIGlobals( drawContext );

            m_sky->Draw( drawContext );
        }

        // Apply SSAO
		if (m_settings.EnableSSAO)
		{
			VA_SCOPE_CPUGPU_TIMER_DEFAULTSELECTED(ASSAO, mainContext);

			// set destination render target and no depth
			mainContext.SetRenderTarget(mainColorRT, nullptr, true);

			m_SSAOEffect->Draw(drawContext, *mainDepthRT.get(), !m_settings.DebugShowOpaqueSSAO, m_GBuffer->GetNormalMap().get(), scissorRectForSSAO);

			// restore main render target / depth
			mainContext.SetRenderTarget(mainColorRT, mainDepthRT, true);
		}

        // Debug wireframe
        if( m_settings.ShowWireframe )
        {
            VA_SCOPE_CPUGPU_TIMER( Wireframe, mainContext );
            drawContext.PassType = vaRenderPassType::ForwardDebugWireframe;

            // this sets up global constants
            m_renderingGlobals->SetAPIGlobals( drawContext );

            // m_scene->Draw( drawContext );
            // //for( size_t i = 0; i < frustumCulledTrees.size( ); i++ )
            // //    frustumCulledTrees[i]->Draw( drawContext );
            // 
            // m_testObjectRenderer->DrawMeshes( drawContext, m_testObjects );
            vaRenderMeshManager::GetInstance().Draw( drawContext, m_meshDrawList );
        }

        if( m_simpleShadowMap != nullptr )
        {
            if( m_simpleShadowMap->GetVolumeShadowMapPlugin( ) != NULL )
                m_simpleShadowMap->GetVolumeShadowMapPlugin()->StopUsing( drawContext, m_simpleShadowMap.get() );
            m_simpleShadowMap->StopUsing( drawContext );
        }
    }

    m_meshDrawList.Reset();

    // GBuffer debug
    {
        VA_SCOPE_CPUGPU_TIMER( GbufferDebug, mainContext );

        vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ), m_lighting.get( ) );
        // this sets up global constants
        m_renderingGlobals->SetAPIGlobals( drawContext );

        // remove depth
        mainContext.SetRenderTarget( mainColorRT, nullptr, true );

        // draw debug stuff
        m_GBuffer->RenderDebugDraw( drawContext );

        // restore main render target / depth
        mainContext.SetRenderTarget( mainColorRT, mainDepthRT, true );
    }

    {
        VA_SCOPE_CPU_TIMER( DebugCanvas3D );

        vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ), m_lighting.get( ) );
        // this sets up global constants
        m_renderingGlobals->SetAPIGlobals( drawContext );

        vaDebugCanvas3DBase * canvas3D = GetCanvas3D( );

        //canvas3D->DrawBox( m_debugBoxPos, m_debugBoxSize, 0xFF000000, 0x20FF0000 );
#ifdef _DEBUG
        canvas3D->DrawAxis( vaVector3( 0, 0, 0 ), 100.0f, NULL, 0.3f );
#endif
        m_renderDevice->DrawDebugCanvas3D( drawContext );
    }

    // restore display 
    mainContext.SetRenderTarget( m_renderDevice->GetMainChainColor(), m_renderDevice->GetMainChainDepth(), true );

    // restore camera border expansion hack
    {
        m_camera->SetYFOV( m_settings.CameraYFov );
        m_camera->SetViewportSize( mainViewportBackup.Width, mainViewportBackup.Height );
        m_camera->Tick( 0.0f, false );  // re-tick to restore
    }

    // Final apply to screen
    {
        VA_SCOPE_CPUGPU_TIMER( FinalApply, mainContext );
    
        vaDrawContext drawContext( *m_camera.get( ), mainContext, *m_renderingGlobals.get( ), m_lighting.get( ) );
        // this sets up global constants
        m_renderingGlobals->SetAPIGlobals( drawContext );
    
        m_postProcess->StretchRect( drawContext, *mainColorRT, vaVector4( (float)0, (float)0, (float)0+mainViewport.Width, (float)0+mainViewport.Height ), vaVector4( 0.0f, 0.0f, (float)mainViewport.Width, (float)mainViewport.Height), true );

        mainViewport = mainViewportBackup;
    }

    if( !m_settings.UseSimpleUI )
    {
#ifdef VA_IMGUI_INTEGRATION_ENABLED
        VA_SCOPE_CPU_TIMER( ImGuiInsert );

        ImGui::PushStyleColor( ImGuiCol_CheckMark, ImColor( 0.2f, 0.9f, 0.2f ) );
        // debug info and controls on the left
        {
            //static bool show_test_window = true;
            //static bool show_another_window = false;

            ImVec4 clear_col = ImColor(114, 144, 154);
            static float f = 0.0f;
            ImGui::SetNextWindowPos( ImVec2( 10, 10 ), ImGuiSetCond_FirstUseEver );     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
            ImGui::SetNextWindowSize( ImVec2( 400, 800 ), ImGuiSetCond_FirstUseEver );
            string mainTitle = vaStringTools::SimpleNarrow( m_application->GetSettings( ).AppName );
            if( ImGui::Begin( mainTitle.c_str( ), 0, ImVec2( 0.f, 0.f ), 0.7f, 0 ) )
            {
                if( ImGui::Button( "Switch to simple UI" ) )
                {
                    m_settings.UseSimpleUI = true;
                }

                int sceneSettingsIndex = vaMath::Clamp( (int)m_settings.SceneChoice, 0, (int)SceneSelectionType::MaxCount-1 );
                if( ImGui::Combo( "Demo scene", &sceneSettingsIndex, "Sponza\0SponzaAndDragons\0Sibenik\0SibenikAndDragons\0LostEmpire\0\0" ) )   // Combo using values packed in a single constant string (for really quick combo)
                {
                    //imguiStateStorage->SetInt( displayTypeID, displayTypeIndex );
                }
                m_settings.SceneChoice = (SceneSelectionType)(sceneSettingsIndex);

                ImGui::Checkbox( "Disable texturing", &m_settings.DisableTexturing );
                if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Just disables scene texturing, but keeps lighting (including normalmaps, if any)" );

                ImGui::Checkbox( "Show wireframe", &m_settings.ShowWireframe );
                float yfov = m_settings.CameraYFov / (VA_PIf) * 360.0f ;
                if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Wireframe" );

                ImGui::InputFloat( "CameraFOV", &yfov, 5.0f, 0.0f, 1 );
                m_settings.CameraYFov = vaMath::Clamp( yfov, 20.0f, 140.0f ) * (VA_PIf) / 360.0f;
                if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Camera Y field of view" );

                ImGui::Separator( );

                ImGui::Checkbox( "Enable SSAO", &m_settings.EnableSSAO );
                ImGui::Checkbox( "SSAO opaque blend", &m_settings.DebugShowOpaqueSSAO );

                // let the ImgUI controls have input priority
                if( !ImGui::GetIO( ).WantCaptureKeyboard )
                {
                }

                ImGui::Separator();

                ImGui::SetNextWindowCollapsed( true );

                ImGui::Separator( );
            	
            	vaImguiHierarchyObject::DrawCollapsable( *m_GBuffer.get( ) );
            }
            ImGui::End( );
        }

        
        m_application->HelperUIDraw();
        ImGui::PopStyleColor( 1 );
    #endif
    }
    else
    {
        // Simple UI

        if( ImGetBigClearSansBold() == nullptr )
        {
            VA_LOG( "Something's wrong with ImGetBigClearSansBold()" );
        }
        else
        {
            ImFont * fontToUse  = ImGetBigClearSansBold();

            ImGui::PushFont( fontToUse );

            ImVec2 approxFontCharDisplaySize = fontToUse->CalcTextSizeA( fontToUse->FontSize, FLT_MAX, 0.0f, "A" );

            float internalMargin = approxFontCharDisplaySize.x * 0.5f;

            float sizeX = approxFontCharDisplaySize.x * 30.0f - internalMargin;
            float sizeY = (float)mainViewport.Height - internalMargin * 2.0f;

            int colorPopsRequired = SetImIntelColours( );

            // ImGui::PushStyleColor( ImGuiCol_FrameBgActive, colIntelBlueButtonActive );
            // ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, colIntelBlueButtonHovered );

            //ImGui::GetStyle().ItemInnerSpacing;

            static float f = 0.0f;
            ImGui::SetNextWindowPos( ImVec2( (float)( mainViewport.Width - sizeX - internalMargin ), internalMargin ), ImGuiSetCond_Always );     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
            ImGui::SetNextWindowSize( ImVec2( (float)sizeX, (float)sizeY ), ImGuiSetCond_Always );
            ImGui::SetNextWindowCollapsed( false, ImGuiSetCond_Always );
            if( ImGui::Begin( "Adaptive SSAO", 0, ImVec2( 0.f, 0.f ), 0.02f, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse ) )
            {
                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 4, 9 ) );

                const vaNestedProfilerNode * profilerNode = vaProfiler::GetInstance().FindNode( "ASSAO" );

                //const vaNestedProfilerNode * profilerNodeRoot = vaProfiler::GetInstance().FindNode( "groot" );

                float infoPanelHeight = approxFontCharDisplaySize.y * 8.0f;

                //ImGui::SetNextWindowPos( ImVec2( internalMargin, internalMargin ), ImGuiSetCond_Always );
                ImGui::BeginChild( ImGui::GetID("Test"), ImVec2( 0.0f, infoPanelHeight ), true, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse );

                bool mouseHover = (vaInputMouseBase::GetCurrent( )->TimeFromLastMove( ) > 1.0f) && (vaInputMouseBase::GetCurrent( )->TimeFromLastMove( ) < 10.0f);

                string adapterName = vaStringTools::SimpleNarrow( m_renderDevice->GetAdapterNameShort() );

                {
                    float textOffsetX = fontToUse->CalcTextSizeA( fontToUse->FontSize, FLT_MAX, 0.0f, "GPU:  " ).x;

                    {
                        float frameTimeMs = (float)m_application->GetAvgFrametime() * 1000.0f;
                        if( m_application->GetVsync() )
                            ImGui::Text( "Frame time: %.1fms (vsync on)", frameTimeMs );
                        else
                            ImGui::Text( "Frame time: %.1fms (vsync off)", frameTimeMs );
                    }

                    ImGui::Text( "Res:"); 
                    ImGui::SameLine( textOffsetX );
                    ImGui::Text( "%d x %d", mainViewport.Width, mainViewport.Height );

                    ImGui::Text( "GPU:");
                    ImGui::SameLine( textOffsetX );
                    ImGui::Text( "%s", adapterName.c_str() );
                }

                if( profilerNode != nullptr )
                { 
                    float assaoAvgTimeMS = (float)(profilerNode->GetFrameAverageTotalTimeGPU() * 1000.0);
                    float assaoCurrTimeMS = (float)(profilerNode->GetFrameLastTotalTimeGPU() * 1000.0);

                    // graph
                    if( !m_application->GetVsync() )
                    {
                        assert( profilerNode->GetFrameHistoryLength() == vaNestedProfilerNode::c_historyFrameCount );
                        const double * originalFrameTimes = profilerNode->GetFrameHistoryTotalTimeGPU();
                        int originalFrameTimesHistoryLast = profilerNode->GetFrameHistoryLastIndex( );
                        float frameTimeMax = 0.0f;
                        float frameTimeMin = FLT_MAX;
                        float frameTimesMS[ vaNestedProfilerNode::c_historyFrameCount ];
                        float frameTimeAvg = 0.0f;
                        for( int i = 0; i < vaNestedProfilerNode::c_historyFrameCount; i++ )
                        {
                            frameTimesMS[i] = (float)originalFrameTimes[(i+originalFrameTimesHistoryLast) % vaNestedProfilerNode::c_historyFrameCount] * 1000.0f;
                            frameTimeMax = vaMath::Max( frameTimeMax, frameTimesMS[i] );
                            frameTimeMin = vaMath::Min( frameTimeMin, frameTimesMS[i] );
                            frameTimeAvg += frameTimesMS[i];
                        }
                        frameTimeAvg /= (float)vaNestedProfilerNode::c_historyFrameCount;

                        static float avgFrametimeGraphMax = 1.0f;
                        avgFrametimeGraphMax = vaMath::Lerp( avgFrametimeGraphMax, frameTimeMax * 1.5f, 0.05f ) + 0.01f;
                        avgFrametimeGraphMax = vaMath::Min( 1000.0f, vaMath::Max( avgFrametimeGraphMax, frameTimeMax * 1.2f ) );

                        float graphWidth = sizeX - 110.0f;

                        string assaoFrameTime = vaStringTools::Format( " ASSAO avg. time: % 2.2f ms", assaoAvgTimeMS );
                        // if( debugOverlay )
                        //     assaoFrameTime = "<disable 'Debug overlay' for profiling>";

                        ImGui::PushStyleColor( ImGuiCol_PlotLines, ImVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
                        
                        float graphTitleCursorPos = ImGui::GetCursorPosY( );
                        
                        ImGui::PlotLines( "", frameTimesMS, _countof(frameTimesMS), 0, nullptr, 0.0f, avgFrametimeGraphMax, ImGui::GetContentRegionAvail() );
                        ImGui::PopStyleColor( 1 );
                        
                        float prevCursorPosY = ImGui::GetCursorPosY( );

                        ImGui::PushFont( nullptr );

                        string minmaxInfoStr = vaStringTools::Format( "min: %.2f, max: %.2f, curr: %.2f ", frameTimeMin, frameTimeMax, assaoCurrTimeMS );
                        ImGui::SetCursorPosY( prevCursorPosY - ImGui::GetTextLineHeightWithSpacing() * 1.3f );
                        ImGui::SetCursorPosX( ImGui::GetContentRegionMax().x - ImGui::CalcTextSize( minmaxInfoStr.c_str() ).x );
                        ImGui::Text( minmaxInfoStr.c_str() );
                        ImGui::PopFont( );

                        ImGui::SetCursorPosY( graphTitleCursorPos );
                        ImGui::TextUnformatted( assaoFrameTime.c_str( ) );

                        ImGui::SetCursorPosY( prevCursorPosY );
                    }
                    else
                    {
                        ImGui::Text( "stats unavailable (VSYNC on)" );
                    }
                }
                else
                { 
                    ImGui::Text( "stats unavailable" );
                }

                ImGui::EndChild();

                ASSAO_Settings & ssaoSettings = m_SSAOEffect->GetSettings();

                float framePadding = ImGui::GetStyle().FramePadding.x;
                float halfwidth = ImGui::GetContentRegionAvailWidth() / 2 - framePadding * 0.5f;

                float buttonSizeY = ImGui::GetCursorPosY();

                ImGuiToggleButton( "Effect enabled", ImVec2( ImGui::GetContentRegionAvailWidth(), 0.0f ), m_settings.EnableSSAO );
                if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Enable/disable ASSAO" );
                buttonSizeY = ImGui::GetCursorPosY() - buttonSizeY;

                //ImGui::Separator();
                ImGui::PushItemWidth( ImGui::GetContentRegionAvailWidth() );
                ImGui::SliderFloat( "radius", &ssaoSettings.Radius, 0.0f, 2.0f, "Effect radius: %.2f" );
                if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Effect world space radius" );
                ImGui::SliderFloat( "strength", &ssaoSettings.ShadowMultiplier, 0.0f, 2.0f, "Effect strength: %.2f" );
                if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Effect strength: occlusion *= value" );
                ImGui::SliderFloat( "power", &ssaoSettings.ShadowPower, 0.0f, 5.0f, "Effect power: %.2f" );
                if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Effect power: occlusion = pow( occlusion, value )" );
                ImGui::SliderFloat( "detailstrength", &ssaoSettings.DetailShadowStrength, 0.0f, 5.0f, "Detail effect strength: %.2f" );
                if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Used for additional two-pixel wide AO; warning: high values cause aliasing" );

                {
                    ImGui::PushItemWidth( halfwidth );
                    ImGui::SliderInt( "bluramount", &ssaoSettings.BlurPassCount, 0, 6, "Blur amount: %.0f" );
                    if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Amount of edge-aware smart blur" );
                    ImGui::SameLine( halfwidth + framePadding * 3.0f );
                    ImGui::SliderFloat( "sharpness", &ssaoSettings.Sharpness, 0.5f, 1.0f, "Sharpness: %.3f" );
                    if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "For edge-aware smart blur; 1.0 means no bleeding over edges, anything less means more blur over edges" );
                    ImGui::PopItemWidth( );
                }

                bool texturingEnabled = !m_settings.DisableTexturing;
                ImGuiToggleButton( "Texturing enabled", ImVec2( ImGui::GetContentRegionAvailWidth(), 0.0f ), texturingEnabled );
                if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Enable/disable texturing to better expose the effect." );
                m_settings.DisableTexturing = !texturingEnabled;

                ImGui::PopItemWidth( );

                int qualityPreset = ssaoSettings.QualityLevel;
                float comboSizeY = ImGui::GetCursorPosY();
                ImGui::PushItemWidth( ImGui::GetContentRegionAvailWidth() ); //ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Quality").x - ImGui::GetStyle().ItemSpacing.x );

                // extension for "Lowest"
                int qualityLevelUI = ssaoSettings.QualityLevel+1;
                if( ssaoSettings.SkipHalfPixelsOnLowQualityLevel ) qualityLevelUI--;
                ImGui::Combo( "Quality", &qualityLevelUI, "Quality: LOWEST\0Quality: LOW\0Quality: MEDIUM\0Quality: HIGH\0Quality: HIGHEST (adaptive)\0\0" ); // Combo using values packed in a single constant string (for really quick combo)
                ImGui::PopItemWidth( );
                // extension for "Lowest"
                ssaoSettings.QualityLevel = vaMath::Clamp( qualityLevelUI-1, 0, 4 );
                ssaoSettings.SkipHalfPixelsOnLowQualityLevel = qualityLevelUI == 0;
                
                comboSizeY = ImGui::GetCursorPosY() - comboSizeY;
              
                //if( ImGui::Button( qualityText.c_str(), ImVec2( ImGui::GetContentRegionAvailWidth(), 0.0f ) ) )
                //    ssaoSettings.QualityLevel = (ssaoSettings.QualityLevel + 1) % 4;

                if( ssaoSettings.QualityLevel == 3 )
                {
                    ImGui::PushItemWidth( ImGui::GetContentRegionAvailWidth() );

                    ImGui::SliderFloat( "AdaptiveLimit", &ssaoSettings.AdaptiveQualityLimit, 0.0f, 1.0f, "Adaptive target: %.2f" );
                    if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Used to limit the max cost regardless of scene contents" );

                    ImGui::PopItemWidth( );
                }
                else
                {
                    //m_SSAOEffect_DevelopmentVersion->DebugShowSampleHeatmap() = false;
                    ImGui::NewLine();
                }

                bool showChangeScene = false;

                ImGui::SetCursorPosY( sizeY - ImGui::GetStyle().ItemSpacing.y - (showChangeScene?3:2) * buttonSizeY - comboSizeY );

                if( showChangeScene && ImGui::Button( "Change scene", ImVec2( ImGui::GetContentRegionAvailWidth(), 0.0f ) ) )
                {
                    if( m_settings.SceneChoice == ASSAODemo::SceneSelectionType::SibenikAndDragons )
                    {
                        SaveCamera( 8 );
                        LoadCamera( 7 );
                        m_settings.SceneChoice = ASSAODemo::SceneSelectionType::SponzaAndDragons;
                    }
                    else
                    {
                        SaveCamera( 7 );
                        LoadCamera( 8 );
                        m_settings.SceneChoice = ASSAODemo::SceneSelectionType::SibenikAndDragons;
                    }
                }

                ImGuiToggleButton( "Animate camera", ImVec2( ImGui::GetContentRegionAvailWidth(), 0.0f ), m_flythroughCameraEnabled );
                if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Enable/disable camera flyhrough; When disabled, right\nmouse click on screen for mouse+WSAD fly mode." );


                {
                    bool fullscreen = m_application->IsFullscreen();
                    bool vsync = m_application->GetVsync();
                    ImGuiToggleButton( "Fullscreen", ImVec2( halfwidth, 0.0f ), fullscreen );
                    if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Toggle fullscreen (windowed borderless)" );
                    ImGui::SameLine( halfwidth + framePadding * 3.0f );
                    ImGuiToggleButton( "Vsync", ImVec2( halfwidth, 0.0f ), vsync );
                    if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Toggle vsync" );
                    m_application->SetFullscreen( fullscreen );
                    m_application->SetVsync( vsync );
                }

                // ImGui::PushItemWidth( ImGui::GetContentRegionAvailWidth() ); //ImGui::PushItemWidth( ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Debug overlay").x - ImGui::GetStyle().ItemSpacing.x );
                // ImGui::Combo( "DebugOverlay", &debugOverlay, "Debug overlay: Disabled\0Debug overlay: Edges\0Debug overlay: Normals\0Debug overlay: Sample heatmap\0\0" );   // Combo using values packed in a single constant string (for really quick combo)
                // ImGui::PopItemWidth( );

                if( ImGui::Button( "Switch to advanced UI", ImVec2( ImGui::GetContentRegionAvailWidth(), 0.0f ) ) )
                {
                    m_settings.UseSimpleUI = false;
                }
                //if( mouseHover && ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Switch to more detailed, more complex UI with various options" );

                // m_SSAOEffect_DevelopmentVersion->DebugShowEdges() = m_SSAOEffect_DevelopmentVersion->DebugShowNormals() = m_SSAOEffect_DevelopmentVersion->DebugShowSampleHeatmap() = false;
                // switch( debugOverlay )
                // {
                // case 0: break;
                // case 1: m_SSAOEffect_DevelopmentVersion->DebugShowEdges() = true;           break;
                // case 2: m_SSAOEffect_DevelopmentVersion->DebugShowNormals() = true;         break;
                // case 3: m_SSAOEffect_DevelopmentVersion->DebugShowSampleHeatmap() = true;   break;
                // default: assert( false ); break;
                // }

                ImGui::PopStyleVar( );
            }
            ImGui::End();

            float prevSizeX = sizeX;

            sizeX = (float)mainViewport.Width - sizeX;//approxFontCharDisplaySize.x * 85.0f;
            sizeY = approxFontCharDisplaySize.y * 1.5f;

            ImGui::SetNextWindowPos( ImVec2( 0.0f, (float)( mainViewport.Height - sizeY ) ), ImGuiSetCond_Always );     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
            ImGui::SetNextWindowSize( ImVec2( (float)sizeX, (float)sizeY ), ImGuiSetCond_Always );
            ImGui::SetNextWindowCollapsed( false, ImGuiSetCond_Always );
            if( ImGui::Begin( "InfoWindow", 0, ImVec2( 0.f, 0.f ), 0.0f, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse ) )
            {
                ImGui::PushStyleColor( ImGuiCol_Text, colIntelBlueButton );
                //ImGui::Text( "ADAPTIVE SCREEN SPACE AMBIENT OCCLUSION, filip.strugar@intel.com" );
                ImGui::SameLine();

                ImGui::PushFont( nullptr );
                string versionStr = "-";
                ImVec2 versionStrSize = ImGui::CalcTextSize( versionStr.c_str() );
                ImGui::SetCursorPosX( ImGui::GetContentRegionMax().x - versionStrSize.x );
                ImGui::SetCursorPosY( ImGui::GetContentRegionMax().y - versionStrSize.y );
                ImGui::Text( versionStr.c_str() );
                ImGui::PopFont( );

                ImGui::PopStyleColor();
            }
            ImGui::End();

            ImGui::PopStyleColor( colorPopsRequired );

            ImGui::PopFont();
        }
    }

    {
#ifdef VA_IMGUI_INTEGRATION_ENABLED
        if( m_application->IsIMGUIVisible( ) )
            ImGui::Render();
#endif
    }
}

void VertexAsylum::ASSAODemo::Initialize( const std::shared_ptr<vaRenderDevice> & renderDevice, const std::shared_ptr<vaApplication> & application )
{
    m_renderDevice = renderDevice; 
    m_application = application;

    auto & params = m_application->GetCommandLineParameters();
//    for each ( auto param in params )
//    {
//        if( param.Switch == L"autobench" )
//            m_autoRunBenchmark = true;
//        if( param.Switch == L"benchpath" )
//            m_benchmarkOutPath = param.Value;
//    }
     m_application->HelperUISettings( ).Flags   = vaApplication::HelperUIFlags( (int)vaApplication::HelperUIFlags::ShowStatsGraph           | 
                                                                                (int)vaApplication::HelperUIFlags::ShowResolutionOptions    | 
                                                                                (int)vaApplication::HelperUIFlags::ShowGPUProfiling         |
//                                                                                (int)vaApplication::HelperUIFlags::ShowCPUProfiling         |
#ifdef _DEBUG
                                                                                (int)vaApplication::HelperUIFlags::ShowAssetsInfo           |
#endif
                                                                                (int)vaApplication::HelperUIFlags::ShowConsoleWindow 
     );
    m_application->HelperUISettings( ).GPUProfilerDefaultOpen  = true;
}



