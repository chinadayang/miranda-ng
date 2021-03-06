/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (�) 2012-15 Miranda NG project (http://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "..\..\core\commonheaders.h"
#include "genmenu.h"

#define STR_SEPARATOR _T("-----------------------------------")

extern bool bIconsDisabled;
extern int DefaultImageListColorDepth;
void RebuildProtoMenus(int);

/////////////////////////////////////////////////////////////////////////////////////////

struct MenuItemOptData : public MZeroedObject
{
	~MenuItemOptData() {}

	int    pos;

	ptrT   name;
	ptrT   defname;
	ptrA   uniqname;
	
	bool   bShow;
	int    id;

	PMO_IntMenuItem pimi;
};

static int SortMenuItems(const MenuItemOptData *p1, const MenuItemOptData *p2)
{
	if (p1->pos < p2->pos) return -1;
	if (p1->pos > p2->pos) return 1;
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

class CGenMenuOptionsPage : public CDlgBase
{
	int iInitMenuValue;

	void SaveTree()
	{
		int MenuObjectId;
		if (!GetCurrentMenuObjectID(MenuObjectId))
			return;

		TCHAR idstr[100];

		TVITEMEX tvi;
		tvi.hItem = m_menuItems.GetRoot();
		tvi.cchTextMax = SIZEOF(idstr);
		tvi.mask = TVIF_TEXT | TVIF_PARAM | TVIF_HANDLE | TVIF_IMAGE;
		tvi.pszText = idstr;

		int count = 0;
		int menupos = GetMenuObjbyId(MenuObjectId);
		if (menupos == -1)
			return;

		TIntMenuObject *pimo = g_menus[menupos];

		char MenuNameItems[256];
		mir_snprintf(MenuNameItems, SIZEOF(MenuNameItems), "%s_Items", pimo->pszName);
		int runtimepos = 100;

		while (tvi.hItem != NULL) {
			m_menuItems.GetItem(&tvi);
			MenuItemOptData *iod = (MenuItemOptData*)tvi.lParam;
			if (iod->pimi) {
				char menuItemName[256], DBString[300];
				GetMenuItemName(iod->pimi, menuItemName, sizeof(menuItemName));

				mir_snprintf(DBString, SIZEOF(DBString), "%s_visible", menuItemName);
				db_set_b(NULL, MenuNameItems, DBString, tvi.iImage != 0);

				mir_snprintf(DBString, SIZEOF(DBString), "%s_pos", menuItemName);
				db_set_dw(NULL, MenuNameItems, DBString, runtimepos);

				mir_snprintf(DBString, SIZEOF(DBString), "%s_name", menuItemName);
				if (iod->name != NULL && iod->defname != NULL &&
					mir_tstrcmp(iod->name, iod->defname) != 0)
					db_set_ts(NULL, MenuNameItems, DBString, iod->name);
				else
					db_unset(NULL, MenuNameItems, DBString);

				runtimepos += 100;
			}

			if (iod->name && !mir_tstrcmp(iod->name, STR_SEPARATOR) && tvi.iImage)
				runtimepos += SEPARATORPOSITIONINTERVAL;

			tvi.hItem = m_menuItems.GetNextSibling(tvi.hItem);
			count++;
		}
	}

	void FreeTreeData()
	{
		HTREEITEM hItem = m_menuItems.GetRoot();
		while (hItem != NULL) {
			TVITEMEX tvi;
			tvi.mask = TVIF_HANDLE | TVIF_PARAM;
			tvi.hItem = hItem;
			m_menuItems.GetItem(&tvi);
			delete (MenuItemOptData *)tvi.lParam;

			tvi.lParam = 0;
			m_menuItems.SetItem(&tvi);

			hItem = m_menuItems.GetNextSibling(hItem);
		}
	}

	void RebuildCurrent()
	{
		int MenuObjectID;
		if (GetCurrentMenuObjectID(MenuObjectID))
			BuildTree(MenuObjectID, true);
	}

	bool BuildTree(int MenuObjectId, bool bReread)
	{
		char buf[256];

		FreeTreeData();
		m_menuItems.DeleteAllItems();

		int menupos = GetMenuObjbyId(MenuObjectId);
		if (menupos == -1)
			return false;

		TIntMenuObject* pimo = g_menus[menupos];
		if (pimo->m_items.first == NULL)
			return false;

		char menuItemName[256], MenuNameItems[256];
		mir_snprintf(MenuNameItems, SIZEOF(MenuNameItems), "%s_Items", pimo->pszName);

		LIST<MenuItemOptData> arItems(10, SortMenuItems);

		for (PMO_IntMenuItem p = pimo->m_items.first; p != NULL; p = p->next) {
			if (p->mi.root != (HGENMENU)-1 && p->mi.root != NULL)
				continue;

			MenuItemOptData *PD = new MenuItemOptData();
			GetMenuItemName(p, menuItemName, sizeof(menuItemName));
			{
				DBVARIANT dbv;
				mir_snprintf(buf, SIZEOF(buf), "%s_name", menuItemName);

				if (!db_get_ts(NULL, MenuNameItems, buf, &dbv)) {
					PD->name = mir_tstrdup(dbv.ptszVal);
					db_free(&dbv);
				}
				else PD->name = mir_tstrdup(GetMenuItemText(p));
			}

			PD->pimi = p;
			PD->defname = mir_tstrdup(GetMenuItemText(p));

			mir_snprintf(buf, SIZEOF(buf), "%s_visible", menuItemName);
			PD->bShow = db_get_b(NULL, MenuNameItems, buf, 1) != 0;

			if (bReread) {
				mir_snprintf(buf, SIZEOF(buf), "%s_pos", menuItemName);
				PD->pos = db_get_dw(NULL, MenuNameItems, buf, 1);
			}
			else PD->pos = (PD->pimi) ? PD->pimi->originalPosition : 0;

			PD->id = p->iCommand;

			if (p->UniqName)
				PD->uniqname = mir_strdup(p->UniqName);

			arItems.insert(PD);
		}

		m_menuItems.SendMsg(WM_SETREDRAW, FALSE, 0);
		int lastpos = 0;
		bool bIsFirst = TRUE;

		TVINSERTSTRUCT tvis;
		tvis.hParent = NULL;
		tvis.hInsertAfter = TVI_LAST;
		tvis.item.mask = TVIF_PARAM | TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

		for (int i = 0; i < arItems.getCount(); i++) {
			MenuItemOptData *PD = arItems[i];
			if (PD->pos - lastpos >= SEPARATORPOSITIONINTERVAL) {
				MenuItemOptData *sep = new MenuItemOptData();
				sep->id = -1;
				sep->name = mir_tstrdup(STR_SEPARATOR);
				sep->pos = PD->pos - 1;

				tvis.item.lParam = (LPARAM)sep;
				tvis.item.pszText = sep->name;
				tvis.item.iImage = tvis.item.iSelectedImage = 1;
				m_menuItems.InsertItem(&tvis);
			}

			tvis.item.lParam = (LPARAM)PD;
			tvis.item.pszText = PD->name;
			tvis.item.iImage = tvis.item.iSelectedImage = PD->bShow;

			HTREEITEM hti = m_menuItems.InsertItem(&tvis);
			if (bIsFirst) {
				m_menuItems.SelectItem(hti);
				bIsFirst = false;
			}

			lastpos = PD->pos;
		}

		m_menuItems.SendMsg(WM_SETREDRAW, TRUE, 0);

		ShowWindow(m_warning.GetHwnd(), (pimo->m_bUseUserDefinedItems) ? SW_HIDE : SW_SHOW);
		m_menuItems.Enable(pimo->m_bUseUserDefinedItems);
		m_btnInsert.Enable(pimo->m_bUseUserDefinedItems);
		return 1;
	}

	bool GetCurrentMenuObjectID(int &result)
	{
		int iItem = m_menuObjects.GetCurSel();
		if (iItem == -1)
			return false;

		result = (int)m_menuObjects.GetItemData(iItem);
		return true;
	}

	CCtrlListBox m_menuObjects;
	CCtrlTreeView m_menuItems;
	CCtrlCheck m_radio1, m_radio2, m_enableIcons;
	CCtrlEdit m_customName, m_service;
	CCtrlButton m_btnInsert, m_btnReset, m_btnSet, m_btnDefault;
	CCtrlBase m_warning;

public:
	CGenMenuOptionsPage() :
		CDlgBase(hInst, IDD_OPT_GENMENU),
		m_menuItems(this, IDC_MENUITEMS),
		m_menuObjects(this, IDC_MENUOBJECTS),
		m_radio1(this, IDC_RADIO1),
		m_radio2(this, IDC_RADIO2),
		m_enableIcons(this, IDC_DISABLEMENUICONS),
		m_btnInsert(this, IDC_INSERTSEPARATOR),
		m_btnReset(this, IDC_RESETMENU),
		m_btnSet(this, IDC_GENMENU_SET),
		m_btnDefault(this, IDC_GENMENU_DEFAULT),
		m_customName(this, IDC_GENMENU_CUSTOMNAME),
		m_service(this, IDC_GENMENU_SERVICE),
		m_warning(this, IDC_NOTSUPPORTWARNING)
	{
		m_btnSet.OnClick = Callback(this, &CGenMenuOptionsPage::btnSet_Clicked);
		m_btnReset.OnClick = Callback(this, &CGenMenuOptionsPage::btnReset_Clicked);
		m_btnInsert.OnClick = Callback(this, &CGenMenuOptionsPage::btnInsert_Clicked);
		m_btnDefault.OnClick = Callback(this, &CGenMenuOptionsPage::btnDefault_Clicked);

		m_menuObjects.OnSelChange = Callback(this, &CGenMenuOptionsPage::onMenuObjectChanged);

		m_menuItems.SetFlags(MTREE_CHECKBOX | MTREE_DND | MTREE_MULTISELECT);
		m_menuItems.OnSelChanged = Callback(this, &CGenMenuOptionsPage::onMenuItemChanged);
	}

	//---- init dialog -------------------------------------------
	virtual void OnInitDialog()
	{
		iInitMenuValue = db_get_b(NULL, "CList", "MoveProtoMenus", TRUE);

		HIMAGELIST himlCheckBoxes = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 2, 2);
		ImageList_AddIcon_IconLibLoaded(himlCheckBoxes, SKINICON_OTHER_NOTICK);
		ImageList_AddIcon_IconLibLoaded(himlCheckBoxes, SKINICON_OTHER_TICK);
		m_menuItems.SetImageList(himlCheckBoxes, TVSIL_NORMAL);

		if (iInitMenuValue)
			m_radio2.SetState(true);
		else
			m_radio1.SetState(true);

		m_enableIcons.SetState(!bIconsDisabled);

		//---- init menu object list --------------------------------------
		for (int i = 0; i < g_menus.getCount(); i++) {
			TIntMenuObject *p = g_menus[i];
			if (p->id != (int)hStatusMenuObject && p->m_bUseUserDefinedItems)
				m_menuObjects.AddString(TranslateTS(p->ptszDisplayName), p->id);
		}
		
		m_menuObjects.SetCurSel(0);
		RebuildCurrent();
	}

	virtual void OnApply()
	{
		bIconsDisabled = m_enableIcons.GetState() == 0;
		db_set_b(NULL, "CList", "DisableMenuIcons", bIconsDisabled);
		SaveTree();

		int iNewMenuValue = !m_radio1.GetState();
		if (iNewMenuValue != iInitMenuValue) {
			db_set_b(NULL, "CList", "MoveProtoMenus", iNewMenuValue);

			RebuildProtoMenus(iNewMenuValue);
			iInitMenuValue = iNewMenuValue;
		}
		RebuildCurrent();
	}

	virtual void OnDestroy()
	{
		ImageList_Destroy(m_menuItems.GetImageList(TVSIL_NORMAL));
		FreeTreeData();
	}

	void btnInsert_Clicked(CCtrlButton*)
	{
		HTREEITEM hti = m_menuItems.GetSelection();
		if (hti == NULL)
			return;

		TVITEMEX tvi = { 0 };
		tvi.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_TEXT;
		tvi.hItem = hti;
		if (!m_menuItems.GetItem(&tvi))
			return;

		MenuItemOptData *PD = new MenuItemOptData();
		PD->id = -1;
		PD->name = mir_tstrdup(STR_SEPARATOR);
		PD->pos = ((MenuItemOptData *)tvi.lParam)->pos - 1;

		TVINSERTSTRUCT tvis = { 0 };
		tvis.item.lParam = (LPARAM)(PD);
		tvis.item.pszText = PD->name;
		tvis.item.iImage = tvis.item.iSelectedImage = 1;
		tvis.hInsertAfter = hti;
		tvis.item.mask = TVIF_PARAM | TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		m_menuItems.InsertItem(&tvis);

		NotifyChange();
	}

	void btnReset_Clicked(CCtrlButton*)
	{
		int MenuObjectID;
		if (GetCurrentMenuObjectID(MenuObjectID)) {
			BuildTree(MenuObjectID, false);
			NotifyChange();
		}
	}

	void btnDefault_Clicked(CCtrlButton*)
	{
		HTREEITEM hti = m_menuItems.GetSelection();
		if (hti == NULL)
			return;

		TVITEMEX tvi;
		tvi.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvi.hItem = hti;
		m_menuItems.GetItem(&tvi);

		MenuItemOptData *iod = (MenuItemOptData *)tvi.lParam;
		if (iod->name && _tcsstr(iod->name, STR_SEPARATOR))
			return;

		iod->name = mir_tstrdup(iod->defname);

		SaveTree();
		RebuildCurrent();
		NotifyChange();
	}

	void btnSet_Clicked(CCtrlButton*)
	{
		HTREEITEM hti = m_menuItems.GetSelection();
		if (hti == NULL)
			return;

		TVITEMEX tvi;
		tvi.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvi.hItem = hti;
		m_menuItems.GetItem(&tvi);

		MenuItemOptData *iod = (MenuItemOptData *)tvi.lParam;
		if (iod->name && _tcsstr(iod->name, STR_SEPARATOR))
			return;

		iod->name = m_customName.GetText();

		SaveTree();
		RebuildCurrent();
		NotifyChange();
	}

	void onMenuObjectChanged(void*)
	{
		m_initialized = false;
		RebuildCurrent();
		m_initialized = true;
	}

	void onMenuItemChanged(void*)
	{
		m_customName.SetTextA("");
		m_service.SetTextA("");

		m_btnDefault.Enable(false);
		m_btnSet.Enable(false);
		m_customName.Enable(false);

		HTREEITEM hti = m_menuItems.GetSelection();
		if (hti == NULL)
			return;

		TVITEMEX tvi;
		tvi.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvi.hItem = hti;
		m_menuItems.GetItem(&tvi);
		if (tvi.lParam == 0)
			return;

		MenuItemOptData *iod = (MenuItemOptData *)tvi.lParam;
		if (iod->name && _tcsstr(iod->name, STR_SEPARATOR))
			return;

		m_customName.SetText(iod->name);

		if (iod->pimi->submenu.first == NULL && iod->uniqname)
			m_service.SetTextA(iod->uniqname);

		m_btnDefault.Enable(mir_tstrcmp(iod->name, iod->defname) != 0);
		m_btnSet.Enable(true);
		m_customName.Enable(true);
	}
};

int GenMenuOptInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = { 0 };
	odp.position = -1000000000;
	odp.pszTitle = LPGEN("Menus");
	odp.pszGroup = LPGEN("Customize");
	odp.flags = ODPF_BOLDGROUPS;
	odp.pDialog = new CGenMenuOptionsPage();
	Options_AddPage(wParam, &odp);
	
	return ProtocolOrderOptInit(wParam, 0);
}
