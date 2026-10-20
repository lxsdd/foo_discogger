#include "stdafx.h"

#include "CGdiPlusBitmap.h"
#include "icon_map.h"

HBITMAP LoadDpiBitmapResource(Icon icon, bool isDark) {

	HBITMAP h_bitmap = nullptr;

	CGdiPlusBitmapResource rec_image;

	auto hInst = core_api::get_my_instance();

	const CSize DPI = QueryScreenDPIEx();
	t_uint32 dpiX = DPI.cx;
	t_uint32 dpiY = DPI.cy;
	int iconWidth = 16;
	int iconHeight = 16;
	int scaledIconWidth = MulDiv(iconWidth, dpiX, USER_DEFAULT_SCREEN_DPI);
	if (scaledIconWidth <= 0) return nullptr;
	int scaledIconHeight = MulDiv(iconHeight, dpiY, USER_DEFAULT_SCREEN_DPI);
	if (scaledIconHeight <= 0) return nullptr;

	const IconMapping* mapping = nullptr;

	mapping = &ICON_RESOURCE_MAPPINGS_COLOR;
	const auto mapped = mapping->find(icon);
	if (mapped == mapping->end() || mapped->second.empty()) return nullptr;
	const auto& iconSizeMappins = mapped->second;

	auto match = std::find_if(
		iconSizeMappins.begin(), iconSizeMappins.end(), [scaledIconWidth, scaledIconHeight](auto entry) {
			return scaledIconWidth <= entry.first && scaledIconHeight <= entry.first;
		});

	if (match == iconSizeMappins.end())
	{
		match = std::prev(iconSizeMappins.end());
	}

	if (!rec_image.Load(MAKEINTRESOURCE(match->second), L"PNG", hInst) || !rec_image.m_pBitmap) return nullptr;

	if (match->first == scaledIconWidth && match->first == scaledIconHeight)
	{
		Gdiplus::Status res_get = rec_image.m_pBitmap->GetHBITMAP(Gdiplus::Color(255, 255, 255)/*Color::Black*/, &h_bitmap);
		rec_image.Empty();
	}
	else {
		auto scaledBitmap = std::make_unique<Gdiplus::Bitmap>(scaledIconWidth, scaledIconHeight);
		scaledBitmap->SetResolution(rec_image.m_pBitmap->GetHorizontalResolution(), rec_image.m_pBitmap->GetVerticalResolution());

		Gdiplus::Graphics graphics(scaledBitmap.get());

		float scalingFactorX = static_cast<float>(scaledIconWidth) / static_cast<float>(match->first);
		float scalingFactorY = static_cast<float>(scaledIconHeight) / static_cast<float>(match->first);
		graphics.ScaleTransform(scalingFactorX, scalingFactorY);
		graphics.DrawImage(rec_image.m_pBitmap, 0, 0);
		rec_image.Empty();
		Gdiplus::Status res_get = scaledBitmap->GetHBITMAP(Gdiplus::Color(255, 255, 255), &h_bitmap);
	}

	return h_bitmap;
}


HICON LoadDpiIconResource(Icon icon, bool isDark) {

	HICON h_icon = nullptr;

	CGdiPlusBitmapResource rec_image;

	auto hInst = core_api::get_my_instance();

	const CSize DPI = QueryScreenDPIEx();
	t_uint32 dpiX = DPI.cx;
	t_uint32 dpiY = DPI.cy;
	int iconWidth = 16;
	int iconHeight = 16;
	int scaledIconWidth = MulDiv(iconWidth, dpiX, USER_DEFAULT_SCREEN_DPI);
	int scaledIconHeight = MulDiv(iconHeight, dpiY, USER_DEFAULT_SCREEN_DPI);

	const IconMapping* mapping = nullptr;

	mapping = &ICON_RESOURCE_MAPPINGS_COLOR;
	const auto mapped = mapping->find(icon);
	if (mapped == mapping->end() || mapped->second.empty()) return nullptr;
	const auto& iconSizeMappins = mapped->second;

	auto match = std::find_if(
		iconSizeMappins.begin(), iconSizeMappins.end(), [scaledIconWidth, scaledIconHeight](auto entry) {
			return scaledIconWidth <= entry.first && scaledIconHeight <= entry.first;
		});

	if (match == iconSizeMappins.end())
	{
		match = std::prev(iconSizeMappins.end());
	}

	if (!rec_image.Load(MAKEINTRESOURCE(match->second), L"PNG", hInst) || !rec_image.m_pBitmap) return nullptr;

	if (match->first == scaledIconWidth && match->first == scaledIconHeight)
	{
		Gdiplus::Status res_get = rec_image.m_pBitmap->GetHICON(&h_icon);
	}
	else {
		auto scaledBitmap = std::make_unique<Gdiplus::Bitmap>(scaledIconWidth, scaledIconHeight);
		scaledBitmap->SetResolution(rec_image.m_pBitmap->GetHorizontalResolution(), rec_image.m_pBitmap->GetVerticalResolution());

		Gdiplus::Graphics graphics(scaledBitmap.get());

		float scalingFactorX = static_cast<float>(scaledIconWidth) / static_cast<float>(match->first);
		float scalingFactorY = static_cast<float>(scaledIconHeight) / static_cast<float>(match->first);
		graphics.ScaleTransform(scalingFactorX, scalingFactorY);
		graphics.DrawImage(rec_image.m_pBitmap, 0, 0);
		rec_image.Empty();
		Gdiplus::Status res_get = scaledBitmap->GetHICON(&h_icon);
	}

	return h_icon;
}
