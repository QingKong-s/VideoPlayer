#pragma once

class CVePlayList : public Dui::CElem
{
	friend class CWndMain;
private:
	CWndMain& m_WndMain;
	Dui::CButton m_BTClose{};
	Dui::CTabList m_List{};
	ID2D1SolidColorBrush* m_pBrush{};
public:
	CVePlayList(CWndMain& w) : m_WndMain{ w } {}

	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};