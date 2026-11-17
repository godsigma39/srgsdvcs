// Main screen: sidebar, module cards, generated settings panel.
//
// Port of MainUI.lua -> C++ / Dear ImGui. The static chrome and its geometry come from the provided
// layout; everything inside the scroll area and the settings panel is generated from the registry in
// VapeModules.hpp, which is why adding a module needs no code here.
//
// Differences from the source file, all forced by being an overlay:
//   - no GLFW window, no OpenGL loop, no swapchain: this draws inside the game's frame
//   - dragging moves the panel within the frame rather than calling glfwSetWindowPos
//   - the card list is the registry, filtered by the search box, not a baked-in array
//   - the settings panel renders real controls for the selected module's settings
//   - Escape hides the menu instead of quitting an application

#include "VapeUI.hpp"
#include "VapeModules.hpp"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cfloat>
#include <string>
#include <vector>

#include <ImGui/imgui_internal.h>

// wingdi.h defines RGB as a 3-argument macro and the provided layout uses a 4-argument colour
// helper with the same name. Drop the macro; the function below is the one everything wants.
#undef RGB

namespace VapeUI
{

// ---------------------------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------------------------

static constexpr float kW = 1338.f;
static constexpr float kH = 722.f;
static constexpr float kPx = 0.845f;      // Roblox TextSize -> em, calibrated against the original

static constexpr float kScrollX = 222.f;
static constexpr float kScrollY = 112.f;
static constexpr float kScrollW = 597.f;
static constexpr float kScrollH = 598.f;

static constexpr float kCardX = 238.f;
static constexpr float kCardW = 565.f;
static constexpr float kCardH = 66.f;
static constexpr float kCardY0 = 12.f;     // relative to the scroll frame
static constexpr float kCardPitch = 86.f;

// Settings panel (Frame_4 in the layout: 509 x 541 at 819,152).
static constexpr float kPanelX = 819.f;
static constexpr float kPanelY = 152.f;
static constexpr float kPanelW = 509.f;
static constexpr float kPanelH = 541.f;

static constexpr float kRowX = 830.3f;
static constexpr float kRowW = 480.f;
static constexpr float kRowH = 46.f;
static constexpr float kRowPitch = 54.f;
static constexpr float kDescY = 204.f;     // module description line, under "Current Selected"
static constexpr float kRowY0 = 236.f;     // first row
static constexpr float kClipTop = 232.f;   // rows scroll inside [kClipTop, kClipBottom]
static constexpr float kClipBottom = 686.f;

// Palette
#define VC_ACCENT  203 , 215 , 253
#define VC_ON      0 , 176 , 92

enum Align { kLeft, kCenter };

// Names kept identical to the provided layout so the .inc fragments drop in unchanged.
static ImFont* gReg = nullptr;
static ImFont* gBold = nullptr;
static ImFont* gSSB = nullptr;
static ImFont* gIcons = nullptr;

static ImDrawList* dl = nullptr;
static ImVec2 org;

static auto P( float x , float y ) -> ImVec2 { return ImVec2( org.x + x , org.y + y ); }

static auto RGB( int r , int g , int b , float a = 1.f ) -> ImU32
{
	return IM_COL32( r , g , b , static_cast<int>( a * 255.f ) );
}

// hhea (ascent - descent) / em, measured from each font.
static auto LineH( ImFont* f ) -> float
{
	return f == gSSB ? 1.326f : ( f == gIcons ? 1.f : 1.219f );
}

static auto Rect( float x , float y , float w , float h , ImU32 c , float r ) -> void
{
	if ( r < 0.f )
		r = ( w < h ? w : h ) * .5f;   // UDim scale radius -> pill / circle

	dl->AddRectFilled( P( x , y ) , P( x + w , y + h ) , c , r );
}

static auto Text( ImFont* f , float rsz , float x , float y , float w , float h , ImU32 c , Align a ,
                  const char* t , int gradient = 0 ) -> void
{
	if ( !f || !t )
		return;

	const float sz = rsz * kPx * LineH( f );
	const ImVec2 ts = f->CalcTextSizeA( sz , FLT_MAX , 0.f , t );

	const float tx = ( a == kLeft ) ? x : x + ( w - ts.x ) * .5f;
	const float ty = y + ( h - ts.y ) * .5f;

	const int v0 = dl->VtxBuffer.Size;

	dl->AddText( f , sz , P( tx , ty ) , c , t );

	// UIGradient across the label: (203,215,253) -> (225,226,227)
	if ( gradient )
	{
		ImGui::ShadeVertsLinearColorGradientKeepAlpha( dl , v0 , dl->VtxBuffer.Size , P( x , y ) ,
		                                               P( x + w , y ) , RGB( 203 , 215 , 253 ) ,
		                                               RGB( 225 , 226 , 227 ) );
	}
}

static auto Icon( const char* glyph , float x , float y , float box , ImU32 c ) -> void
{
	if ( !gIcons )
		return;

	const float s = box * 0.92f;

	x += ( box - s ) * .5f;
	y += ( box - s ) * .5f;

	const ImVec2 ts = gIcons->CalcTextSizeA( s , FLT_MAX , 0.f , glyph );

	dl->AddText( gIcons , s , P( x + ( s - ts.x ) * .5f , y + ( s - ts.y ) * .5f ) , c , glyph );
}

// ---------------------------------------------------------------------------------------------
// static chrome
// ---------------------------------------------------------------------------------------------

static const float kSide[][4] =
{
#include "layout_side.inc"
};

static const int kNumSide = sizeof( kSide ) / sizeof( kSide[ 0 ] );

static auto DrawStatic() -> void
{
#include "layout_static_vape.inc"
}

// ---------------------------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------------------------

static int    g_Tab = 0;
static Module* g_pSelected = nullptr;
static float  g_Scroll = 0.f;
static char   g_Search[ 64 ] = "";
static float  g_PanelScroll = 0.f;
static bool   g_DragWindow = false;

// Keybind capture: one at a time, tracked by module name so it survives list rebuilds.
static std::string g_RebindModule;
static std::string g_RebindSetting;

// True while we are waiting for every key and button to come back up. Starting a capture from a
// click means the click itself is still held, and without this the first thing captured would be
// the mouse button that opened the capture.
static bool g_RebindWaitRelease = false;

static auto KeyName( int nVk , char* szOut , size_t cbOut ) -> void
{
	if ( nVk == 0 )
	{
		strncpy_s( szOut , cbOut , "-" , _TRUNCATE );
		return;
	}

	// Mouse buttons are not in the keyboard name table; label them ourselves.
	switch ( nVk )
	{
		case VK_LBUTTON:  strncpy_s( szOut , cbOut , "Mouse 1" , _TRUNCATE ); return;
		case VK_RBUTTON:  strncpy_s( szOut , cbOut , "Mouse 2" , _TRUNCATE ); return;
		case VK_MBUTTON:  strncpy_s( szOut , cbOut , "Mouse 3" , _TRUNCATE ); return;
		case VK_XBUTTON1: strncpy_s( szOut , cbOut , "Mouse 4" , _TRUNCATE ); return;
		case VK_XBUTTON2: strncpy_s( szOut , cbOut , "Mouse 5" , _TRUNCATE ); return;
		default: break;
	}

	const UINT uScan = MapVirtualKeyA( static_cast<UINT>( nVk ) , MAPVK_VK_TO_VSC ) << 16;

	if ( GetKeyNameTextA( static_cast<LONG>( uScan ) , szOut , static_cast<int>( cbOut ) ) <= 0 )
		snprintf( szOut , cbOut , "0x%02X" , nVk );
}

// One-shot capture. Returns the newly pressed virtual key, or -1.
static auto CaptureKey() -> int
{
	// Release gate: nothing counts as "the key the user chose" until everything is released after
	// the click that started the capture.
	if ( g_RebindWaitRelease )
	{
		for ( int nVk = 1 ; nVk < 256 ; ++nVk )
		{
			if ( GetAsyncKeyState( nVk ) & 0x8000 )
				return -1;
		}

		g_RebindWaitRelease = false;
		return -1;
	}

	for ( int nVk = 1 ; nVk < 256 ; ++nVk )
	{
		// Modifiers alone are legal bindings.
		if ( GetAsyncKeyState( nVk ) & 0x8000 )
			return nVk;
	}

	return -1;
}

// ---------------------------------------------------------------------------------------------
// small drawing helpers
// ---------------------------------------------------------------------------------------------

// Material Icons glyphs (UTF-8 bytes, so no source-encoding surprises).
#define GLYPH_CHEVRON_L "\xee\x97\x8b"
#define GLYPH_CHEVRON_R "\xee\x97\x8c"
#define GLYPH_SEARCH    "\xee\xa2\xb6"
#define GLYPH_TUNE      "\xee\x90\xa9"

static auto MixCol( ImU32 a , ImU32 b , float t ) -> ImU32
{
	const ImVec4 A = ImGui::ColorConvertU32ToFloat4( a );
	const ImVec4 B = ImGui::ColorConvertU32ToFloat4( b );

	return ImGui::ColorConvertFloat4ToU32( ImVec4( A.x + ( B.x - A.x ) * t , A.y + ( B.y - A.y ) * t ,
	                                               A.z + ( B.z - A.z ) * t , A.w + ( B.w - A.w ) * t ) );
}

// Per-element animation value that eases toward flTarget. Keyed by an address plus a slot number,
// stored in the window's state storage, so no per-widget bookkeeping is needed anywhere.
static auto Anim( const void* pKey , int nSlot , float flTarget , float flSpeed = 16.f ) -> float
{
	const ImGuiID Id = ImGui::GetID( pKey ) + static_cast<ImGuiID>( nSlot ) * 2654435761u;

	float* pVal = ImGui::GetStateStorage()->GetFloatRef( Id , flTarget );

	*pVal += ( flTarget - *pVal ) * ( std::min )( 1.f , ImGui::GetIO().DeltaTime * flSpeed );

	return *pVal;
}

static auto TextW( ImFont* f , float rsz , const char* t ) -> float
{
	return f->CalcTextSizeA( rsz * kPx * LineH( f ) , FLT_MAX , 0.f , t ).x;
}

// Right-aligned text whose right edge sits at xRight.
static auto TextR( ImFont* f , float rsz , float xRight , float y , float h , ImU32 c , const char* t ) -> void
{
	const float w = TextW( f , rsz , t );

	Text( f , rsz , xRight - w , y , w , h , c , kLeft , t );
}

// Pill switch, 42 x 22, animated.
static auto DrawSwitch( const void* pKey , float x , float y , bool bOn , bool bHovered ) -> void
{
	const float t = Anim( pKey , 0 , bOn ? 1.f : 0.f , 18.f );

	const float w = 42.f;
	const float h = 22.f;

	const ImU32 Off = bHovered ? RGB( 78 , 78 , 80 ) : RGB( 64 , 64 , 66 );

	Rect( x , y , w , h , MixCol( Off , RGB( VC_ON ) , t ) , h * .5f );

	// soft glow when on
	if ( t > 0.02f )
		dl->AddRect( P( x - 1.f , y - 1.f ) , P( x + w + 1.f , y + h + 1.f ) , RGB( VC_ON , .35f * t ) ,
		             h * .5f + 1.f , 0 , 2.f );

	dl->AddCircleFilled( P( x + 11.f + t * ( w - 22.f ) , y + h * .5f ) , 8.f , RGB( 246 , 248 , 255 ) );
}

// Inset "chip" used by values, keycaps and dropdowns.
static auto DrawChip( float x , float y , float w , float h , bool bHot , float flGlow = 0.f ) -> void
{
	Rect( x , y , w , h , RGB( 22 , 22 , 23 ) , 8.f );

	const ImU32 Edge = MixCol( bHot ? RGB( 84 , 84 , 88 ) : RGB( 56 , 56 , 58 ) , RGB( VC_ACCENT ) , flGlow );

	dl->AddRect( P( x , y ) , P( x + w , y + h ) , Edge , 8.f , 0 , 1.f );
}

// ---------------------------------------------------------------------------------------------
// settings panel rows
// ---------------------------------------------------------------------------------------------

// Background, hover state and label for one row. When bBand is set the whole row is also a clickable
// item. nId has to be unique among the rows in this window: ImGui identifies items by label, and every
// row here uses the same label, so without PushID they would all collapse into one interactive item.
static auto BeginRow( const void* pKey , int nId , float y , const char* szLabel , bool bBand ,
                      bool* pHovered , bool* pRightClicked = nullptr ) -> bool
{
	bool bClicked = false;
	bool bHovered = false;

	if ( bBand )
	{
		ImGui::PushID( nId );

		ImGui::SetCursorPos( ImVec2( kRowX , y ) );
		ImGui::InvisibleButton( "band" , ImVec2( kRowW , kRowH ) );

		bClicked = ImGui::IsItemClicked( 0 );
		bHovered = ImGui::IsItemHovered();

		if ( pRightClicked )
			*pRightClicked = ImGui::IsItemClicked( 1 );

		ImGui::PopID();
	}
	else
	{
		bHovered = ImGui::IsMouseHoveringRect( P( kRowX , y ) , P( kRowX + kRowW , y + kRowH ) );
	}

	if ( pHovered )
		*pHovered = bHovered;

	const float t = Anim( pKey , 1 , bHovered ? 1.f : 0.f );

	Rect( kRowX , y , kRowW , kRowH , MixCol( RGB( 30 , 30 , 31 ) , RGB( 38 , 38 , 40 ) , t ) , 8.f );

	Text( gSSB , 20.f , kRowX + 16.f , y , 230.f , kRowH , RGB( 240 , 242 , 248 ) , kLeft , szLabel );

	return bClicked;
}

static constexpr float kRight = kRowX + kRowW - 16.f;   // right edge of every row's control

static auto DrawToggle( Module& M , Setting& S , float y , int nId ) -> void
{
	bool bHovered = false;

	if ( BeginRow( &S , nId , y , S.Name.c_str() , true , &bHovered ) )
	{
		S.B = !S.B;
		S.Changed = true;

		if ( M.OnToggle )
			M.OnToggle( M );
	}

	DrawSwitch( &S , kRight - 42.f , y + ( kRowH - 22.f ) * .5f , S.B , bHovered );
}

static auto DrawSlider( Module& M , Setting& S , float y , int nId ) -> void
{
	(void)M;

	BeginRow( &S , nId , y , S.Name.c_str() , false , nullptr );

	// value chip, right
	char szValue[ 64 ] = { 0 };

	if ( S.Step >= 1.f )
		snprintf( szValue , sizeof( szValue ) , "%.0f%s" , S.F , S.Suffix.c_str() );
	else if ( S.Step >= 0.1f )
		snprintf( szValue , sizeof( szValue ) , "%.1f%s" , S.F , S.Suffix.c_str() );
	else
		snprintf( szValue , sizeof( szValue ) , "%.2f%s" , S.F , S.Suffix.c_str() );

	const float ChipW = ( std::max )( 62.f , TextW( gSSB , 18.f , szValue ) + 22.f );
	const float ChipX = kRight - ChipW;

	const float TrackX = kRowX + 200.f;
	const float TrackW = ChipX - 16.f - TrackX;
	const float TrackY = y + kRowH * .5f;

	ImGui::PushID( nId );

	ImGui::SetCursorPos( ImVec2( TrackX - 8.f , y + 6.f ) );
	ImGui::InvisibleButton( "slider" , ImVec2( TrackW + 16.f , kRowH - 12.f ) );

	const bool bActive = ImGui::IsItemActive();
	const bool bHovered = ImGui::IsItemHovered();

	ImGui::PopID();

	if ( bActive )
	{
		const float flMouseX = ImGui::GetIO().MousePos.x - org.x;

		float flNorm = ( flMouseX - TrackX ) / TrackW;

		flNorm = ( flNorm < 0.f ) ? 0.f : ( flNorm > 1.f ) ? 1.f : flNorm;

		float flValue = S.Min + flNorm * ( S.Max - S.Min );

		// Snap to the step so the displayed value and the stored value agree.
		if ( S.Step > 0.f )
			flValue = S.Min + std::round( ( flValue - S.Min ) / S.Step ) * S.Step;

		flValue = ( flValue < S.Min ) ? S.Min : ( flValue > S.Max ) ? S.Max : flValue;

		if ( flValue != S.F )
		{
			S.F = flValue;
			S.Changed = true;
		}
	}

	const float flSpan = ( S.Max - S.Min );
	const float flNorm = ( flSpan > 0.f ) ? ( ( S.F - S.Min ) / flSpan ) : 0.f;

	const float flHot = Anim( &S , 2 , ( bHovered || bActive ) ? 1.f : 0.f );
	const float flKx = TrackX + TrackW * flNorm;

	// track + fill
	Rect( TrackX , TrackY - 2.5f , TrackW , 5.f , RGB( 52 , 52 , 55 ) , 2.5f );

	if ( flKx > TrackX + 1.f )
	{
		const int v0 = dl->VtxBuffer.Size;

		dl->AddRectFilled( P( TrackX , TrackY - 2.5f ) , P( flKx , TrackY + 2.5f ) , IM_COL32_WHITE , 2.5f );

		ImGui::ShadeVertsLinearColorGradientKeepAlpha( dl , v0 , dl->VtxBuffer.Size , P( TrackX , 0 ) ,
		                                               P( TrackX + TrackW , 0 ) , RGB( 120 , 138 , 215 ) ,
		                                               RGB( VC_ACCENT ) );
	}

	// knob with halo
	if ( flHot > 0.01f )
		dl->AddCircleFilled( P( flKx , TrackY ) , 7.f + 5.f * flHot , RGB( VC_ACCENT , .16f * flHot ) );

	dl->AddCircleFilled( P( flKx , TrackY ) , 7.f , RGB( 240 , 244 , 255 ) );

	// value chip
	DrawChip( ChipX , y + 8.f , ChipW , kRowH - 16.f , bHovered || bActive , flHot );

	Text( gSSB , 18.f , ChipX , y + 8.f , ChipW , kRowH - 16.f , RGB( VC_ACCENT ) , kCenter , szValue );
}

static auto DrawCombo( Module& M , Setting& S , float y , int nId ) -> void
{
	(void)M;

	bool bHovered = false;
	bool bRight = false;

	const bool bClicked = BeginRow( &S , nId , y , S.Name.c_str() , true , &bHovered , &bRight );

	const float ChipW = 200.f;
	const float ChipX = kRight - ChipW;
	const float ChipY = y + 8.f;
	const float ChipH = kRowH - 16.f;

	const int nCount = static_cast<int>( S.Items.size() );

	if ( nCount > 0 && ( bClicked || bRight ) )
	{
		// Left third steps back, anywhere else steps forward. Right-click always steps back.
		const float flMouseX = ImGui::GetIO().MousePos.x - org.x;
		const bool bBack = bRight || ( flMouseX < ChipX + ChipW * .3f && flMouseX >= ChipX );

		S.I = ( ( ( S.I + ( bBack ? -1 : 1 ) ) % nCount ) + nCount ) % nCount;
		S.Changed = true;
	}

	DrawChip( ChipX , ChipY , ChipW , ChipH , bHovered , 0.f );

	const char* szValue = ( nCount == 0 ) ? "-" : S.Items[ ( S.I < 0 || S.I >= nCount ) ? 0 : S.I ].c_str();

	const ImU32 Arrow = bHovered ? RGB( VC_ACCENT ) : RGB( 110 , 110 , 116 );

	Icon( GLYPH_CHEVRON_L , ChipX + 4.f , ChipY + ( ChipH - 22.f ) * .5f , 22.f , Arrow );
	Icon( GLYPH_CHEVRON_R , ChipX + ChipW - 26.f , ChipY + ( ChipH - 22.f ) * .5f , 22.f , Arrow );

	dl->PushClipRect( P( ChipX + 28.f , ChipY ) , P( ChipX + ChipW - 28.f , ChipY + ChipH ) , true );

	Text( gSSB , 18.f , ChipX + 28.f , ChipY , ChipW - 56.f , ChipH , RGB( 255 , 255 , 255 ) , kCenter , szValue );

	dl->PopClipRect();
}

static auto DrawKeybind( Module& M , Setting& S , float y , int nId ) -> void
{
	bool bHovered = false;
	bool bRight = false;

	const bool bCapturing = ( g_RebindModule == M.Name && g_RebindSetting == S.Name );

	if ( BeginRow( &S , nId , y , S.Name.c_str() , true , &bHovered , &bRight ) )
	{
		if ( bCapturing )
		{
			g_RebindModule.clear();
			g_RebindSetting.clear();
		}
		else
		{
			g_RebindModule = M.Name;
			g_RebindSetting = S.Name;
			g_RebindWaitRelease = true;
		}
	}

	if ( bCapturing )
	{
		const int nVk = CaptureKey();

		if ( nVk > 0 )
		{
			// Escape clears the bind rather than being bound to Escape, which would be useless.
			S.Key = ( nVk == VK_ESCAPE ) ? 0 : nVk;
			S.Changed = true;

			g_RebindModule.clear();
			g_RebindSetting.clear();
		}
		else if ( bRight )
		{
			g_RebindModule.clear();
			g_RebindSetting.clear();
		}
	}
	else if ( bRight && S.Key != 0 )
	{
		// Right-click on a bound row clears it.
		S.Key = 0;
		S.Changed = true;
	}

	char szKey[ 64 ] = { 0 };

	if ( bCapturing )
		strncpy_s( szKey , "press a key..." , _TRUNCATE );
	else
		KeyName( S.Key , szKey , sizeof( szKey ) );

	const float ChipW = ( std::max )( 96.f , TextW( gSSB , 18.f , szKey ) + 30.f );
	const float ChipX = kRight - ChipW;

	// pulse while waiting for input
	const float flPulse = bCapturing ? ( .55f + .45f * std::sin( static_cast<float>( ImGui::GetTime() ) * 6.f ) ) : 0.f;

	DrawChip( ChipX , y + 8.f , ChipW , kRowH - 16.f , bHovered , flPulse );

	const bool bUnbound = ( S.Key == 0 && !bCapturing );

	Text( gSSB , 18.f , ChipX , y + 8.f , ChipW , kRowH - 16.f ,
	      bCapturing ? RGB( VC_ACCENT ) : ( bUnbound ? RGB( 120 , 120 , 126 ) : RGB( 255 , 255 , 255 ) ) ,
	      kCenter , szKey );
}

static auto DrawColor( Module& M , Setting& S , float y , int nId ) -> void
{
	(void)M;

	BeginRow( &S , nId , y , S.Name.c_str() , false , nullptr );

	const float BarW = 128.f;
	const float BarX = kRight - BarW;
	const float SwW = 38.f;
	const float SwX = BarX - 16.f - SwW;

	// swatch with a hairline so dark colours stay visible on a dark row
	const ImU32 Current = ImGui::ColorConvertFloat4ToU32( ImVec4( S.Col[ 0 ] , S.Col[ 1 ] , S.Col[ 2 ] , 1.f ) );

	Rect( SwX , y + 8.f , SwW , kRowH - 16.f , Current , 8.f );

	dl->AddRect( P( SwX , y + 8.f ) , P( SwX + SwW , y + kRowH - 8.f ) , RGB( 255 , 255 , 255 , .20f ) ,
	             8.f , 0 , 1.f );

	// three stacked horizontal channel sliders
	for ( int i = 0 ; i < 3 ; ++i )
	{
		const float ty = y + 11.f + i * 12.f;

		ImGui::PushID( nId * 4 + i );

		ImGui::SetCursorPos( ImVec2( BarX - 4.f , ty - 6.f ) );
		ImGui::InvisibleButton( "col" , ImVec2( BarW + 8.f , 12.f ) );

		const bool bActive = ImGui::IsItemActive();
		const bool bHovered = ImGui::IsItemHovered();

		ImGui::PopID();

		if ( bActive )
		{
			float flNorm = ( ImGui::GetIO().MousePos.x - org.x - BarX ) / BarW;

			flNorm = ( flNorm < 0.f ) ? 0.f : ( flNorm > 1.f ) ? 1.f : flNorm;

			S.Col[ i ] = flNorm;
			S.Changed = true;
		}

		const ImU32 Tint = ( i == 0 ) ? RGB( 224 , 84 , 84 ) : ( i == 1 ) ? RGB( 84 , 214 , 124 ) : RGB( 96 , 134 , 242 );

		Rect( BarX , ty - 2.f , BarW , 4.f , RGB( 52 , 52 , 55 ) , 2.f );
		Rect( BarX , ty - 2.f , BarW * S.Col[ i ] , 4.f , Tint , 2.f );

		dl->AddCircleFilled( P( BarX + BarW * S.Col[ i ] , ty ) , ( bActive || bHovered ) ? 5.f : 4.f ,
		                     RGB( 245 , 247 , 255 ) );
	}
}

static auto DrawSettingsPanel( Module* pModule ) -> void
{
	const ImGuiIO& io = ImGui::GetIO();

	if ( !pModule )
	{
		Icon( GLYPH_TUNE , kPanelX + kPanelW * .5f - 20.f , 330.f , 40.f , RGB( 64 , 64 , 68 ) );

		Text( gReg , 14.f , kPanelX , 380.f , kPanelW , 24.f , RGB( 110 , 110 , 116 ) , kCenter ,
		      "Select a module to edit its settings" );

		return;
	}

	// Selected module name, next to "Current Selected:".
	Text( gBold , 14.f , 868.4f , 152.f , 200.f , 50.f , RGB( 255 , 255 , 255 ) , kCenter ,
	      pModule->Name.c_str() );

	// Description sits on its own line under the header; it used to overlap the header row.
	if ( !pModule->Desc.empty() )
	{
		dl->PushClipRect( P( kRowX , kDescY ) , P( kRowX + kRowW , kDescY + 24.f ) , true );

		Text( gReg , 12.f , kRowX + 4.f , kDescY , kRowW , 24.f , RGB( 124 , 124 , 130 ) , kLeft ,
		      pModule->Desc.c_str() );

		dl->PopClipRect();
	}

	// ---- scrolling -----------------------------------------------------------------------
	const int nRows = 1 + static_cast<int>( pModule->Settings.size() );

	const float flView = kClipBottom - kClipTop;
	const float flContent = ( kRowY0 - kClipTop ) + nRows * kRowPitch;
	const float flMaxScroll = ( flContent > flView ) ? ( flContent - flView ) : 0.f;

	const bool bOverPanel = ImGui::IsMouseHoveringRect( P( kPanelX , kClipTop ) ,
	                                                    P( kPanelX + kPanelW , kClipBottom ) );

	if ( bOverPanel && io.MouseWheel != 0.f && flMaxScroll > 0.f )
		g_PanelScroll -= io.MouseWheel * 40.f;

	g_PanelScroll = std::clamp( g_PanelScroll , 0.f , flMaxScroll );

	// Clip both drawing and hit-testing so rows scrolled under the header cannot be clicked.
	ImGui::PushClipRect( P( kPanelX , kClipTop ) , P( kPanelX + kPanelW , kClipBottom ) , true );

	// Master enable row first, always. Every module has one, and it is the thing the user reaches
	// for most, so it should not be buried under its own settings.
	{
		const float y = kRowY0 - g_PanelScroll;

		bool bHovered = false;

		if ( BeginRow( pModule , 1000 , y , "Enabled" , true , &bHovered ) )
		{
			pModule->Enabled = !pModule->Enabled;

			if ( pModule->OnToggle )
				pModule->OnToggle( *pModule );
		}

		DrawSwitch( pModule , kRight - 42.f , y + ( kRowH - 22.f ) * .5f , pModule->Enabled , bHovered );
	}

	int nRow = 1;

	for ( auto& S : pModule->Settings )
	{
		const float y = kRowY0 + nRow * kRowPitch - g_PanelScroll;

		if ( y + kRowH >= kClipTop && y <= kClipBottom )
		{
			switch ( S.Kind )
			{
				case WKind::Toggle:  DrawToggle( *pModule , S , y , nRow );  break;
				case WKind::Slider:  DrawSlider( *pModule , S , y , nRow );  break;
				case WKind::Combo:   DrawCombo( *pModule , S , y , nRow );   break;
				case WKind::Keybind: DrawKeybind( *pModule , S , y , nRow ); break;
				case WKind::Color:   DrawColor( *pModule , S , y , nRow );   break;
			}
		}

		++nRow;
	}

	ImGui::PopClipRect();

	// slim scroll indicator on the panel's right edge
	if ( flMaxScroll > 0.f )
	{
		const float flThumbH = ( std::max )( 30.f , flView * flView / flContent );
		const float flThumbY = kClipTop + ( g_PanelScroll / flMaxScroll ) * ( flView - flThumbH );

		Rect( kPanelX + kPanelW - 8.f , flThumbY , 3.f , flThumbH , RGB( 255 , 255 , 255 , .22f ) , 1.5f );
	}
}

// ---------------------------------------------------------------------------------------------
// main screen
// ---------------------------------------------------------------------------------------------

static auto MatchesSearch( const Module& M ) -> bool
{
	if ( g_Search[ 0 ] == '\0' )
		return true;

	char szNeedle[ 64 ] = { 0 };
	char szHaystack[ 128 ] = { 0 };

	for ( int i = 0 ; g_Search[ i ] && i < 63 ; ++i )
		szNeedle[ i ] = static_cast<char>( tolower( static_cast<unsigned char>( g_Search[ i ] ) ) );

	for ( int i = 0 ; i < static_cast<int>( M.Name.size() ) && i < 127 ; ++i )
		szHaystack[ i ] = static_cast<char>( tolower( static_cast<unsigned char>( M.Name[ i ] ) ) );

	return strstr( szHaystack , szNeedle ) != nullptr;
}

auto DrawMainScreen() -> void
{
	const ImGuiIO& io = ImGui::GetIO();

	if ( gReg == nullptr )
	{
		gReg = FontRegular();
		gBold = FontBold();
		gSSB = FontSSB();
		gIcons = FontIcons();
	}

	const ImVec2 Pos = EnsureMenuPos( kW , kH );

	// Dim the game behind the panel.
	ImGui::GetBackgroundDrawList()->AddRectFilled( ImVec2( 0 , 0 ) , io.DisplaySize ,
	                                               IM_COL32( 0 , 0 , 0 , 110 ) );

	ImGui::SetNextWindowPos( Pos );
	ImGui::SetNextWindowSize( ImVec2( kW , kH ) );

	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding , ImVec2( 0 , 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize , 0.f );
	ImGui::PushStyleColor( ImGuiCol_WindowBg , ImVec4( 0 , 0 , 0 , 0 ) );

	ImGui::Begin( "##vape_main" , nullptr ,
	              ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
	              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
	              ImGuiWindowFlags_NoScrollWithMouse );

	dl = ImGui::GetWindowDrawList();
	org = ImGui::GetWindowPos();

	// soft drop shadow behind the frame (the original had one; an overlay has to draw it itself)
	for ( int i = 0 ; i < 6 ; ++i )
	{
		const float e = 3.f + i * 4.f;

		dl->AddRectFilled( P( -e , -e + 8.f ) , P( kW + e , kH + e + 8.f ) , RGB( 0 , 0 , 0 , .045f ) , 8.f + e );
	}

	// ---- frame background: rounded, horizontal gradient right(30,30,30) -> left(53,54,54) -----
	{
		const int v0 = dl->VtxBuffer.Size;

		dl->AddRectFilled( org , P( kW , kH ) , IM_COL32_WHITE , 8.f );

		ImGui::ShadeVertsLinearColorGradientKeepAlpha( dl , v0 , dl->VtxBuffer.Size , org , P( kW , 0 ) ,
		                                               RGB( 53 , 54 , 54 ) , RGB( 30 , 30 , 30 ) );
	}

	// ---- scroll frame background -------------------------------------------------------------
	dl->AddRectFilled( P( kScrollX , kScrollY ) , P( kScrollX + kScrollW , kScrollY + kScrollH ) ,
	                   RGB( 0 , 0 , 0 , .15f ) );

	// ---- current tab's modules ---------------------------------------------------------------
	auto& Registry = GetRegistry();

	const Tab eTab = static_cast<Tab>( g_Tab );

	std::vector<Module*> Visible;

	for ( auto* pM : Registry.InTab( eTab ) )
	{
		if ( MatchesSearch( *pM ) )
			Visible.push_back( pM );
	}

	const float flContentH = ( Visible.empty() ? 0.f : kCardY0 + static_cast<float>( Visible.size() ) * kCardPitch );
	const float flMaxScroll = ( flContentH > kScrollH ) ? ( flContentH - kScrollH ) : 0.f;

	const ImVec2 ScrollMin = P( kScrollX , kScrollY );
	const ImVec2 ScrollMax = P( kScrollX + kScrollW , kScrollY + kScrollH );

	const bool bOverScroll = ImGui::IsMouseHoveringRect( ScrollMin , ScrollMax );

	if ( bOverScroll && io.MouseWheel != 0.f )
		g_Scroll = g_Scroll - io.MouseWheel * 45.f;

	g_Scroll = std::clamp( g_Scroll , 0.f , flMaxScroll );

	// ---- static chrome -----------------------------------------------------------------------
	DrawStatic();

	// Live Steam name in the header.
	{
		char szGreeting[ 160 ] = { 0 };
		snprintf( szGreeting , sizeof( szGreeting ) , "Welcome %s" , SteamName() );

		Text( gReg , 18.f , 1154.8f , 12.f , 200.f , 50.f , RGB( 255 , 255 , 255 ) , kCenter ,
		      szGreeting );
	}

	// ---- cards -------------------------------------------------------------------------------
	dl->PushClipRect( ScrollMin , ScrollMax , true );

	if ( Visible.empty() )
	{
		Icon( GLYPH_SEARCH , kScrollX + kScrollW * .5f - 20.f , kScrollY + 210.f , 40.f , RGB( 70 , 70 , 74 ) );

		Text( gReg , 14.f , kScrollX , kScrollY + 262.f , kScrollW , 24.f , RGB( 112 , 112 , 118 ) , kCenter ,
		      g_Search[ 0 ] ? "No modules match your search" : "Nothing in this category yet" );
	}

	for ( int i = 0 ; i < static_cast<int>( Visible.size() ) ; ++i )
	{
		Module* pM = Visible[ i ];

		const float y = kScrollY + kCardY0 + i * kCardPitch - g_Scroll;

		if ( y + kCardH < kScrollY || y > kScrollY + kScrollH )
			continue;   // off-screen; skip the hover test too

		const ImVec2 a = P( kCardX , y );
		const ImVec2 b = P( kCardX + kCardW , y + kCardH );

		const float SwX = kCardX + kCardW - 20.f - 42.f;
		const float SwY = y + ( kCardH - 22.f ) * .5f;

		const bool bHovered = bOverScroll && ImGui::IsMouseHoveringRect( a , b );
		const bool bOverSwitch = bHovered && ImGui::IsMouseHoveringRect( P( SwX - 8.f , SwY - 8.f ) ,
		                                                                 P( SwX + 50.f , SwY + 30.f ) );
		const bool bSelected = ( pM == g_pSelected );

		if ( bHovered && ImGui::IsMouseClicked( 0 ) )
		{
			if ( bOverSwitch )
			{
				pM->Enabled = !pM->Enabled;

				if ( pM->OnToggle )
					pM->OnToggle( *pM );
			}
			else
				g_pSelected = pM;
		}

		const float flHover = Anim( pM , 3 , bHovered ? 1.f : 0.f );
		const float flSel = Anim( pM , 4 , bSelected ? 1.f : 0.f );

		const ImU32 Background = MixCol( MixCol( RGB( 34 , 34 , 35 ) , RGB( 41 , 41 , 43 ) , flHover ) ,
		                                 RGB( 46 , 51 , 67 ) , flSel );

		Rect( kCardX , y , kCardW , kCardH , Background , 8.f );

		if ( flSel > 0.01f )
		{
			dl->AddRect( a , b , RGB( VC_ACCENT , .22f * flSel ) , 8.f , 0 , 1.f );

			// Selection bar, same accent as the sidebar; grows in from the middle.
			const float hh = ( kCardH - 20.f ) * .5f * flSel;

			dl->AddRectFilled( P( kCardX , y + kCardH * .5f - hh ) , P( kCardX + 3.f , y + kCardH * .5f + hh ) ,
			                   RGB( VC_ACCENT ) , 2.f );
		}

		// Text is left-aligned and clipped short of the switch so a long description can never run under it.
		dl->PushClipRect( P( kCardX , y ) , P( SwX - 14.f , y + kCardH ) , true );

		Text( gBold , 20.f , kCardX + 20.f , y + 8.f , kCardW , 28.f , RGB( 255 , 255 , 255 ) , kLeft ,
		      pM->Name.c_str() );

		Text( gReg , 13.f , kCardX + 20.f , y + 36.f , kCardW , 22.f , RGB( 118 , 118 , 124 ) , kLeft ,
		      pM->Desc.c_str() );

		dl->PopClipRect();

		DrawSwitch( pM , SwX , SwY , pM->Enabled , bOverSwitch );
	}

	dl->PopClipRect();

	// ---- scrollbar -----------------------------------------------------------------------------
	if ( flMaxScroll > 0.f )
	{
		const float flTrackX = kScrollX + kScrollW - 10.f;
		const float flThumbH = ( std::max )( 36.f , kScrollH * kScrollH / flContentH );
		const float flThumbY = kScrollY + ( g_Scroll / flMaxScroll ) * ( kScrollH - flThumbH );

		ImGui::SetCursorPos( ImVec2( flTrackX - 4.f , flThumbY ) );
		ImGui::InvisibleButton( "##vape_thumb" , ImVec2( 14.f , flThumbH ) );

		const bool bDrag = ImGui::IsItemActive();

		if ( bDrag )
			g_Scroll = std::clamp( g_Scroll + io.MouseDelta.y * ( flMaxScroll / ( kScrollH - flThumbH ) ) ,
			                       0.f , flMaxScroll );

		const float flHot = Anim( &g_Scroll , 5 , ( ImGui::IsItemHovered() || bDrag ) ? 1.f : 0.f );

		Rect( flTrackX , kScrollY + 4.f , 6.f , kScrollH - 8.f , RGB( 255 , 255 , 255 , .04f ) , 3.f );
		Rect( flTrackX , flThumbY , 6.f , flThumbH , MixCol( RGB( 86 , 88 , 98 ) , RGB( 140 , 148 , 175 ) , flHot ) ,
		      3.f );
	}

	// ---- search box --------------------------------------------------------------------------
	ImGui::PushFont( gReg , 14.f * kPx * 1.219f );
	ImGui::PushStyleColor( ImGuiCol_FrameBg , ImVec4( 0 , 0 , 0 , 0 ) );
	ImGui::PushStyleColor( ImGuiCol_TextDisabled , ImVec4( 1 , 1 , 1 , 1 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding , ImVec2( 0 , 0 ) );

	ImGui::SetCursorPos( ImVec2( 470.f , 16.f + 25.f - ImGui::GetFontSize() * .5f ) );
	ImGui::SetNextItemWidth( 425.f );

	if ( ImGui::InputTextWithHint( "##vape_search" , "Search here" , g_Search , sizeof( g_Search ) ) )
		g_Scroll = 0.f;   // a new filter makes the old offset meaningless

	ImGui::PopStyleVar();
	ImGui::PopStyleColor( 2 );
	ImGui::PopFont();

	// ---- sidebar categories ------------------------------------------------------------------
	if ( g_pSelected == nullptr || g_pSelected->TabIndex != eTab )
	{
		// Keep the panel pointed at something real whenever the tab changes.
		auto InTab = Registry.InTab( eTab );
		g_pSelected = InTab.empty() ? nullptr : InTab.front();
	}

	static Module* s_pLastSelected = nullptr;

	if ( s_pLastSelected != g_pSelected )
	{
		g_PanelScroll = 0.f;   // a different module has a different row list
		s_pLastSelected = g_pSelected;
	}

	bool bOverSide = false;

	for ( int i = 0 ; i < kNumSide ; ++i )
	{
		const ImVec2 a = P( kSide[ i ][ 0 ] , kSide[ i ][ 1 ] );
		const ImVec2 b = P( kSide[ i ][ 0 ] + kSide[ i ][ 2 ] , kSide[ i ][ 1 ] + kSide[ i ][ 3 ] );

		const bool bHovered = ImGui::IsMouseHoveringRect( a , b );

		bOverSide |= bHovered;

		if ( bHovered && ImGui::IsMouseClicked( 0 ) && i != g_Tab )
		{
			g_Tab = i;
			g_Scroll = 0.f;
			g_pSelected = nullptr;   // re-resolved above on the next frame
		}

		// Category label, drawn here because the label offsets depend on the name length.
		Text( gBold , 14.f , kSide[ i ][ 0 ] + 34.f , kSide[ i ][ 1 ] , 130.f , kSide[ i ][ 3 ] ,
		      RGB( 255 , 255 , 255 ) , kLeft , TabName( static_cast<Tab>( i ) ) );

		const float flActive = Anim( &kSide[ i ] , 6 , ( i == g_Tab ) ? 1.f : 0.f );
		const float flHover = Anim( &kSide[ i ] , 7 , bHovered ? 1.f : 0.f );

		if ( flActive > 0.01f )
			dl->AddRectFilled( a , b , RGB( VC_ACCENT , .10f * flActive ) , 8.f );

		if ( flHover > 0.01f && i != g_Tab )
			dl->AddRectFilled( a , b , RGB( 255 , 255 , 255 , .05f * flHover ) , 8.f );

		if ( flActive > 0.01f )
		{
			const float hh = ( kSide[ i ][ 3 ] - 20.f ) * .5f * flActive;
			const float cy = ( a.y + b.y ) * .5f;

			dl->AddRectFilled( ImVec2( a.x , cy - hh ) , ImVec2( a.x + 3.f , cy + hh ) , RGB( VC_ACCENT ) , 2.f );
		}
	}

	// ---- settings panel ----------------------------------------------------------------------
	DrawSettingsPanel( g_pSelected );

	// ---- drag / keys -------------------------------------------------------------------------
	// A drag only moves the window if it started on empty background. Starting it over a card, a
	// slider or a sidebar button used to drag the whole menu along with the control.
	const bool bOverPanel = ImGui::IsMouseHoveringRect( P( kPanelX , kPanelY ) ,
	                                                    P( kPanelX + kPanelW , kPanelY + kPanelH ) );

	if ( ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && !bOverScroll && !bOverSide &&
	     !bOverPanel && ImGui::IsMouseClicked( 0 ) )
	{
		ImGui::SetWindowFocus();
		g_DragWindow = true;
	}

	if ( !ImGui::IsMouseDown( 0 ) )
		g_DragWindow = false;

	if ( g_DragWindow && ImGui::IsMouseDragging( 0 ) )
		MoveMenuBy( io.MouseDelta );

	if ( ImGui::IsKeyPressed( ImGuiKey_Escape ) && g_RebindModule.empty() )
		SetMenuOpen( false );

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar( 2 );
}

} // namespace VapeUI
