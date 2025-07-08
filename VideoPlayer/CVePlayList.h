#pragma once

class CVePlayList : public Dui::CElem
{
	friend class CWndMain;
private:
	CWndMain& m_WndMain;
	Dui::CList m_List{};
public:
	CVePlayList(CWndMain& w) : m_WndMain{ w } {}

};