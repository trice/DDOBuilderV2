// FindGearDialog.cpp
//

#include "stdafx.h"
#include "FindGearDialog.h"
#include "Build.h"
#include "GlobalSupportFunctions.h"
#include "MouseHook.h"
#include "EnableBuddyButton.h"

// CFindGearDialog dialog

namespace
{
    enum ItemListColumns
    {
        ILC_name = 0,
        ILC_level,
    };
}

IMPLEMENT_DYNAMIC(CFindGearDialog, CDialog)

CFindGearDialog::CFindGearDialog(
        CWnd* pParent,
        Build* pBuild) :
    CDialog(CFindGearDialog::IDD, pParent),
    m_pBuild(pBuild),
    m_bInitialising(false),
    m_showingTip(false),
    m_tipCreated(false),
    m_hoverItem(-1),
    m_hoverHandle(0),
    m_bIgnoreNextMessage(false),
    m_levelRange(30)
{
    m_levelRange = AfxGetApp()->GetProfileInt("ItemSelectDialog", "LevelRange", 5);
    memset(m_augmentHookHandles, 0, sizeof(m_augmentHookHandles));
}

CFindGearDialog::~CFindGearDialog()
{
    AfxGetApp()->WriteProfileInt("ItemSelectDialog", "LevelRange", m_levelRange);
}

void CFindGearDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_TEXT, m_editSearchText);
    DDX_Control(pDX, IDC_BUTTON_CLEAR_FILTER, m_clearFilter);
    DDX_Control(pDX, IDC_ITEM_LIST, m_availableItemsCtrl);
    DDX_Control(pDX, IDC_EQUIP_IT, m_buttonEquipIt);
    if (!pDX->m_bSaveAndValidate)
    {
        VERIFY(m_sortHeader.SubclassWindow(m_availableItemsCtrl.GetHeaderCtrl()->GetSafeHwnd()));
    }
    DDX_Control(pDX, IDC_COMBO_WITHINLEVELS, m_comboLevelRange);
    DDX_Control(pDX, IDC_COMBO_ITEMLEVEL, m_comboItemLevel);
    DDX_Control(pDX, IDC_CHECK_IGNORERAIDITEMS, m_buttonIgnoreRaidItems);
    DDX_Control(pDX, IDC_STATIC_AUGMENTS, m_staticAugments);
    for (size_t i = 0; i < MAX_Augments; ++i)
    {
        DDX_Control(pDX, IDC_STATIC_AUGMENT_TYPE1 + i, m_augmentType[i]);
        DDX_Control(pDX, IDC_COMBO_AUGMENT1 + i, m_comboAugmentDropList[i]);
        DDX_Control(pDX, IDC_EDIT_AUGMENT1 + i, m_augmentValues[i]);
        DDX_Control(pDX, IDC_EDIT2_AUGMENT1 + i, m_augmentValues2[i]);
        DDX_Control(pDX, IDC_COMBO_LEVELAUGMENT1 + i, m_comboAugmentLevel[i]);
    }
    DDX_Control(pDX, IDC_STATIC_UPGRADES, m_staticUpgrades);
    for (size_t i = 0; i < MAX_Upgrades; ++i)
    {
        DDX_Control(pDX, IDC_STATIC_UPGRADE_TYPE1 + i, m_upgradeType[i]);
        DDX_Control(pDX, IDC_COMBO_UPGRADE1 + i, m_comboUpgradeDropList[i]);
    }
}

BEGIN_MESSAGE_MAP(CFindGearDialog, CDialog)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_ITEM_LIST, OnItemSelected)
    ON_CONTROL_RANGE(CBN_SELENDOK, IDC_COMBO_AUGMENT1, IDC_COMBO_AUGMENT1 + MAX_Augments - 1, OnAugmentSelect)
    ON_CONTROL_RANGE(CBN_SELENDCANCEL, IDC_COMBO_AUGMENT1, IDC_COMBO_AUGMENT1 + MAX_Augments - 1, OnAugmentCancel)
    ON_CONTROL_RANGE(CBN_SELENDOK, IDC_COMBO_UPGRADE1, IDC_COMBO_UPGRADE1 + MAX_Upgrades - 1, OnUpgradeSelect)
    ON_CONTROL_RANGE(CBN_SELENDCANCEL, IDC_COMBO_UPGRADE1, IDC_COMBO_UPGRADE1 + MAX_Upgrades - 1, OnUpgradeCancel)
    ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_EDIT_AUGMENT1, IDC_EDIT_AUGMENT1 + MAX_Augments - 1, OnKillFocusAugmentEdit)
    ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_EDIT2_AUGMENT1, IDC_EDIT2_AUGMENT1 + MAX_Augments - 1, OnKillFocusAugmentEdit)
    ON_CONTROL_RANGE(CBN_SELENDOK, IDC_COMBO_LEVELAUGMENT1, IDC_COMBO_LEVELAUGMENT1 + MAX_Augments - 1, OnAugmentLevelSelect)
    ON_CONTROL_RANGE(CBN_SELENDCANCEL, IDC_COMBO_LEVELAUGMENT1, IDC_COMBO_LEVELAUGMENT1 + MAX_Augments - 1, OnAugmentLevelCancel)
    ON_WM_SIZE()
    ON_WM_WINDOWPOSCHANGING()
    ON_NOTIFY(HDN_ENDTRACK, IDC_ITEM_LIST, OnEndtrackListItems)
    ON_NOTIFY(HDN_DIVIDERDBLCLICK, IDC_ITEM_LIST, OnEndtrackListItems)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_ITEM_LIST, OnColumnclickListItems)
    ON_NOTIFY(NM_HOVER, IDC_ITEM_LIST, OnHoverListItems)
    ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
    ON_MESSAGE(WM_MOUSEHOVER, OnHoverComboBox)
    ON_MESSAGE(WM_MOUSEENTER, OnMouseEnter)
    ON_BN_CLICKED(IDC_EQUIP_IT, OnButtonEquipIt)
    ON_EN_KILLFOCUS(IDC_EDIT_TEXT, OnSearchTextKillFocus)
    ON_CBN_SELENDOK(IDC_COMBO_ITEMLEVEL, OnItemLevelSelect)
    ON_CBN_SELENDOK(IDC_COMBO_WITHINLEVELS, OnSelEndLevelRange)
    ON_BN_CLICKED(IDC_CHECK_IGNORERAIDITEMS, OnButtonIgnoreRaidItems)
    ON_BN_CLICKED(IDC_BUTTON_CLEAR_FILTER, OnButtonClearFilter)
END_MESSAGE_MAP()

// CFindGearDialog message handlers
BOOL CFindGearDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_bInitialising = true;

    m_tooltip.Create(this);
    m_tipCreated = true;

    int iState = AfxGetApp()->GetProfileInt("ItemSelectDialog", "ExcludeRaidItems", 0);
    if (iState != 0)
    {
        m_buttonIgnoreRaidItems.SetCheck(BST_CHECKED);
    }

    EnableBuddyButton(m_editSearchText.GetSafeHwnd(), m_clearFilter.GetSafeHwnd(), BBS_RIGHT);

    // add list control columns
    m_availableItemsCtrl.InsertColumn(0, "Item Name", LVCFMT_LEFT, 226);
    m_availableItemsCtrl.InsertColumn(1, "Level", LVCFMT_LEFT, 50);
    m_availableItemsCtrl.SetImageList(&ItemsImages(), LVSIL_SMALL);
    m_availableItemsCtrl.SetImageList(&ItemsImages(), LVSIL_NORMAL);
    m_sortHeader.SetSortArrow(1, FALSE);     // sort by level by default
    m_availableItemsCtrl.SetExtendedStyle(
            m_availableItemsCtrl.GetExtendedStyle()
            | LVS_EX_FULLROWSELECT
            | LVS_EX_TRACKSELECT
            | LVS_EX_ONECLICKACTIVATE
            //| LVS_EX_LABELTIP); // stop hover tooltips from working
            );

    m_comboLevelRange.ResetContent();
    m_comboLevelRange.AddString("5");
    m_comboLevelRange.AddString("10");
    m_comboLevelRange.AddString("15");
    m_comboLevelRange.AddString("20");
    m_comboLevelRange.AddString("25");
    m_comboLevelRange.AddString("30");
    m_comboLevelRange.AddString("35");
    m_comboLevelRange.AddString("40");
    switch (m_levelRange)
    {
        case 5:     m_comboLevelRange.SetCurSel(0); break;
        case 10:    m_comboLevelRange.SetCurSel(1); break;
        case 15:    m_comboLevelRange.SetCurSel(2); break;
        case 20:    m_comboLevelRange.SetCurSel(3); break;
        case 25:    m_comboLevelRange.SetCurSel(4); break;
        case 30:    m_comboLevelRange.SetCurSel(5); break;
        case 35:    m_comboLevelRange.SetCurSel(6); break;
        case 40:    m_comboLevelRange.SetCurSel(7); break;
    }

    PopulateAvailableItemList();
    SetupItemLevel(m_pBuild->Level(), m_item.MinLevel(), m_item.HasUserSetsLevel());
    EnableControls();

    // add monitor locations to show augment tooltips
    for (size_t i = 0 ; i < MAX_Augments; ++i)
    {
        CRect rect;
        m_comboAugmentDropList[i].GetWindowRect(&rect);
        rect.DeflateRect(0, 1, 0, 1);   // ensure they do not overlap
        m_augmentHookHandles[i] = GetMouseHook()->AddRectangleToMonitor(
                this->GetSafeHwnd(),
                rect,           // screen coordinates,
                WM_MOUSEENTER,
                WM_MOUSELEAVE,
                false);
    }

    LoadColumnWidthsByName(&m_availableItemsCtrl, "FindGearDialog_%s");
    m_bInitialising = false;
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CFindGearDialog::PopulateAvailableItemList()
{
    CWaitCursor longOperation;

    CString searchText;
    m_editSearchText.GetWindowText(searchText);
    searchText.MakeLower(); // case less text match

    int minItemLevel = m_pBuild->Level() - m_levelRange;
    bool bIgnoreRaidItems = (m_buttonIgnoreRaidItems.GetCheck() == BST_CHECKED);

    const std::list<Item> & allItems = Items();
    m_availableItems.clear();
    std::list<Item>::const_iterator it = allItems.begin();
    while (it != allItems.end())
    {
        if (bIgnoreRaidItems && it->IsRaidItem())
        {
            // ignore this raid item
            ++it;
            continue;
        }
        bool canSelect = true;
        // exclude weapons
        if (!(*it).CanEquipToSlot(Inventory_Weapon1)
                && !(*it).CanEquipToSlot(Inventory_Weapon2)
                && !(*it).CanEquipToSlot(Inventory_CosmeticArmor)
                && !(*it).CanEquipToSlot(Inventory_CosmeticCloak)
                && !(*it).CanEquipToSlot(Inventory_CosmeticHelm)
                && (*it).MinLevel() <= m_pBuild->Level()
                && (static_cast<int>((*it).MinLevel()) >= minItemLevel || (*it).MinLevel() == 0))
        {
            // must have the required search text present in the item
            if (searchText.GetLength() > 0)
            {
                canSelect &= (*it).ContainsSearchText((LPCTSTR)searchText, m_pBuild);
            }
            // some items have requirements to be able to use, see if they are met
            if ((*it).HasRequirementsToUse())
            {
                if (!(*it).RequirementsToUse().Met(*m_pBuild, m_pBuild->Level() - 1, true, Inventory_Unknown, (*it).HasWeapon() ? (*it).Weapon() : Weapon_Unknown, Weapon_Unknown))
                {
                    canSelect = false;
                }
            }
            if (canSelect)
            {
                // user can select this item, add it to the available list
                m_availableItems.push_back(&(*it));
            }
        }
        ++it;
    }
    m_availableItemsCtrl.LockWindowUpdate();
    m_availableItemsCtrl.DeleteAllItems();

    // now populate the control
    size_t itemIndex = 0;
    std::list<const Item*>::const_iterator aiit = m_availableItems.begin();
    while (aiit != m_availableItems.end())
    {
        int item = m_availableItemsCtrl.InsertItem(
                m_availableItemsCtrl.GetItemCount(),
                (*aiit)->Name().c_str(),
                (*aiit)->IconIndex());
        CString level;
        level.Format("%d", (*aiit)->MinLevel());
        m_availableItemsCtrl.SetItemText(item, 1, level);
        m_availableItemsCtrl.SetItemData(item, itemIndex);

        ++itemIndex;
        ++aiit;
    }

    m_availableItemsCtrl.SortItems(
            CFindGearDialog::SortCompareFunction,
            (long)GetSafeHwnd());

    int sel = -1;           // item to be selected
    LVFINDINFO fi;
    fi.flags = LVFI_STRING;
    fi.psz = m_item.Name().c_str();
    sel = m_availableItemsCtrl.FindItem(&fi);
    if (sel >= 0)
    {
        m_availableItemsCtrl.SetSelectionMark(sel);
        m_availableItemsCtrl.SetItemState(sel, LVIS_SELECTED, LVIS_SELECTED);
        m_availableItemsCtrl.EnsureVisible(sel, FALSE);
    }
    m_availableItemsCtrl.UnlockWindowUpdate();
    HideTip();
}

void CFindGearDialog::EnableControls()
{
    // show / hide the available augments based on the selected item
    std::vector<ItemAugment> augments = m_item.Augments();
    ASSERT(augments.size() < MAX_Augments);

    m_staticAugments.ShowWindow(augments.size() > 0 ? SW_SHOW : SW_HIDE);
    for (size_t i = 0; i < MAX_Augments; ++i)
    {
        if (i < augments.size())
        {
            // we have an augment, populate it with the available for this type
            m_augmentType[i].SetWindowText(augments[i].Type().c_str());
            PopulateAugmentList(
                    &m_comboAugmentDropList[i],
                    &m_augmentValues[i],
                    &m_augmentValues2[i],
                    &m_comboAugmentLevel[i],
                    augments[i]);
            m_augmentType[i].ShowWindow(SW_SHOW);
            m_comboAugmentDropList[i].ShowWindow(SW_SHOW);
        }
        else
        {
            // no augment, hide control
            m_augmentType[i].ShowWindow(SW_HIDE);
            m_comboAugmentDropList[i].ShowWindow(SW_HIDE);
            m_augmentValues[i].ShowWindow(SW_HIDE);
            m_augmentValues2[i].ShowWindow(SW_HIDE);
            m_comboAugmentLevel[i].ShowWindow(SW_HIDE);
        }
    }
    // now show any upgrade slots
    size_t upgradeIndex = 0;
    std::vector<SlotUpgrade> upgrades = m_item.SlotUpgrades();
    m_staticUpgrades.ShowWindow(upgrades.size() > 0 ? SW_SHOW : SW_HIDE);
    for (; upgradeIndex < upgrades.size(); ++upgradeIndex)
    {
        // we have a primary upgrade list
        PopulateSlotUpgradeList(upgradeIndex, upgrades[upgradeIndex]);
    }
    // now hide the unused
    for (;upgradeIndex < MAX_Upgrades; ++upgradeIndex)
    {
        m_upgradeType[upgradeIndex].ShowWindow(SW_HIDE);
        m_comboUpgradeDropList[upgradeIndex].ShowWindow(SW_HIDE);
    }
    // disable OK button if we have multiple augments with set suppression
    // (max one per item)
    size_t count = 0;
    const std::vector<ItemAugment> & itemAugments = m_item.Augments();
    std::vector<ItemAugment>::const_iterator it = itemAugments.begin();
    while (it != itemAugments.end())
    {
        if ((*it).HasSelectedAugment())
        {
            const Augment & augment = FindAugmentByName((*it).SelectedAugment(), &m_item);
            if (augment.HasSuppressSetBonus())
            {
                ++count;
            }
        }
        ++it;
    }
    m_buttonEquipIt.EnableWindow(count <= 1);
    if (count > 1)
    {
        AfxMessageBox(
                "Note: You cannot equip an item with multiple augments that suppress set bonuses",
                MB_ICONERROR | MB_OK);
    }
}

void CFindGearDialog::PopulateAugmentList(
    CComboBox* combo,
    CEdit* edit1,
    CEdit* edit2,
    CComboBox* comboLevel,
    const ItemAugment& augment)
{
    combo->LockWindowUpdate();
    combo->ResetContent();
    // get all the augments compatible with this slot type
    std::list<Augment> augments = CompatibleAugments(augment, m_pBuild->Level());
    std::list<Augment>::const_iterator it = augments.begin();
    std::string selectedAugment = augment.HasSelectedAugment()
        ? augment.SelectedAugment()
        : "";

    // note that this list can be sorted
    {
        int index = 0;
        while (it != augments.end())
        {
            int itemIndex = combo->AddString((*it).Name().c_str());
            combo->SetItemData(itemIndex, index);
            ++it;
            ++index;
        }
    }

    if (selectedAugment != "")
    {
        combo->SelectString(0, selectedAugment.c_str());
        Augment selAugment = augment.GetSelectedAugment();
        // this augment is configurable by the user
        if (selAugment.HasEnterValue())
        {
            edit1->ShowWindow(SW_SHOW);
            edit1->EnableWindow(true);
            // show the value of this augment in the control
            CString text;
            text.Format("%.0f", augment.HasValue() ? augment.Value() : 0);
            edit1->SetWindowText(text);
            if (selAugment.HasDualValues())
            {
                edit2->ShowWindow(SW_SHOW);
                edit2->EnableWindow(true);
                // show the value of this augment in the control
                text.Format("%.0f", augment.HasValue2() ? augment.Value2() : 0);
                edit2->SetWindowText(text);
            }
            else
            {
                edit2->ShowWindow(SW_HIDE);
            }
            comboLevel->ShowWindow(SW_HIDE);
        }
        else
        {
            if (selAugment.HasLevelValue())
            {
                // does it come from the augment level combo or the item level one?
                if (selAugment.HasLevels())
                {
                    int selIndex = 0;
                    if (augment.HasSelectedLevelIndex())
                    {
                        selIndex = augment.SelectedLevelIndex();
                    }
                    comboLevel->ResetContent();
                    std::vector<int> levels = selAugment.Levels();
                    for (auto&& lit : levels)
                    {
                        if (lit <= static_cast<int>(m_pBuild->Level()))
                        {
                            // only show selectable levels that are <= build level
                            CString text;
                            text.Format("%d", lit);
                            int index = comboLevel->AddString(text);
                            comboLevel->SetItemData(index, lit);
                        }
                    }
                    comboLevel->SetCurSel(selIndex);
                    comboLevel->ShowWindow(SW_SHOW);
                    CString text;
                    text.Format("%+.0f", selAugment.LevelValue()[selIndex]);
                    edit1->SetWindowText(text);
                    edit1->ShowWindow(SW_SHOW);
                    edit1->EnableWindow(false);     // read only
                    if (selAugment.HasDualValues())
                    {
                        edit2->ShowWindow(SW_SHOW);
                        edit1->EnableWindow(false); // read only
                        // show the value of this augment in the control
                        text.Format("%+.0f", selAugment.LevelValue2()[selIndex]);
                        edit2->SetWindowText(text);
                    }
                    else
                    {
                        edit2->ShowWindow(SW_HIDE);
                    }
                }
                else
                {
                    // value is set by the level of the item we are in
                    CString text;
                    text.Format("%+.0f", selAugment.LevelValue()[m_item.MinLevel() - 1]);
                    edit1->SetWindowText(text);
                    edit1->ShowWindow(SW_SHOW);
                    edit1->EnableWindow(false);     // read only
                    if (selAugment.HasDualValues())
                    {
                        edit2->ShowWindow(SW_SHOW);
                        edit1->EnableWindow(false); // read only
                        // show the value of this augment in the control
                        text.Format("%+.0f", selAugment.LevelValue2()[m_item.MinLevel() - 1]);
                        edit2->SetWindowText(text);
                    }
                    else
                    {
                        edit2->ShowWindow(SW_HIDE);
                    }
                    comboLevel->ShowWindow(SW_HIDE);
                }
            }
            else
            {
                // augment has a fixed bonus
                edit1->ShowWindow(SW_HIDE);
                edit2->ShowWindow(SW_HIDE);
                comboLevel->ShowWindow(SW_HIDE);
            }
        }
    }
    else
    {
        // no augment selected
        edit1->ShowWindow(SW_HIDE);
        edit2->ShowWindow(SW_HIDE);
        comboLevel->ShowWindow(SW_HIDE);
    }
    combo->UnlockWindowUpdate();
}

void CFindGearDialog::OnItemSelected(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (!m_bInitialising)
    {
        NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
        if ((pNMListView->uChanged & LVIF_STATE) 
                && (pNMListView->uNewState & LVIS_SELECTED))
        {
            // item selection has changed, select it
            int sel = pNMListView->iItem;
            sel = m_availableItemsCtrl.GetItemData(sel);
            if (sel >= 0 && sel < (int)m_availableItems.size())
            {
                // must be a different item to the one already selected
                std::list<const Item*>::const_iterator it = m_availableItems.begin();
                std::advance(it, sel);
                if ((*it)->Name() != m_item.Name())
                {
                    m_item = *(*it);
                    AddSpecialSlots(Inventory_Boots, m_item);  // adds reaper or mythic slots to the item
                    // update the other controls
                    EnableControls();
                    // item selected, can now click equip it
                    m_buttonEquipIt.EnableWindow(TRUE);
                    CString text;
                    text.Format("Item Selection and Configuration - %s",
                            m_item.Name().c_str());
                    SetWindowText(text);
                    SetupItemLevel(m_pBuild->Level(), m_item.MinLevel(), m_item.HasUserSetsLevel());
                }
            }
        }
    }
    *pResult = 0;
}

void CFindGearDialog::OnAugmentSelect(UINT nID)
{
    // the user has selected an augment in one of the augment combo boxes
    int augmentIndex = nID - IDC_COMBO_AUGMENT1;
    ASSERT(augmentIndex >= 0 && augmentIndex < (int)m_item.Augments().size());
    int sel = m_comboAugmentDropList[augmentIndex].GetCurSel();
    if (sel != CB_ERR)
    {
        std::vector<ItemAugment> augments = m_item.Augments();
        // user has selected an augment. Get its name
        CString text;
        m_comboAugmentDropList[augmentIndex].GetLBText(sel, text);
        if (text == " No Augment")
        {
            // "No Augment" select, clear any augment
            augments[augmentIndex].ClearSelectedAugment();
            augments[augmentIndex].ClearValue();
        }
        else
        {
            const Augment& oldAugment = augments[augmentIndex].GetSelectedAugment();
            // may need to clear previous augments granted augment slot
            std::string oldSelection;
            if (augments[augmentIndex].HasSelectedAugment())
            {
                oldSelection = augments[augmentIndex].SelectedAugment();
            }
            augments[augmentIndex].SetSelectedAugment((LPCTSTR)text);
            augments[augmentIndex].ClearValue(); // if we just selected an augment ensure any previous value does not carry over
            if (oldSelection != "")
            {
                if (oldAugment.HasGrantAugment())
                {
                    RemoveAugment(&augments, oldAugment.GrantAugment());
                }
                // add a granted augment if required (only granted while augment selected)
                if (oldAugment.HasGrantConditionalAugment())
                {
                    if (oldAugment.HasWeaponClass()
                        && m_item.HasWeapon())
                    {
                        std::string weaponClass = EnumEntryText(oldAugment.WeaponClass(), weaponClassTypeMap);
                        if (m_pBuild->IsWeaponInGroup(weaponClass, m_item.Weapon()))
                        {
                            // this is a new augment slot that needs to be added
                            RemoveAugment(&augments, oldAugment.GrantConditionalAugment());
                        }
                    }
                }
            }
            // if an augment selected has AddAugment fields, add them to the
            // item also if they do not already exist
            const Augment& augment = augments[augmentIndex].GetSelectedAugment();
            std::list<std::string> augmentsToAdd = augment.AddAugment();
            std::list<std::string>::const_iterator it = augmentsToAdd.begin();
            //bool bSuppressSetBonus = augment.HasSuppressSetBonus(); // a single item can only have one augment that does this
            while (it != augmentsToAdd.end())
            {
                AddAugment(&augments, (*it));
                ++it;
            }
            // add a granted augment if required (only granted while augment selected)
            if (augment.HasGrantAugment())
            {
                // this is a new augment slot that needs to be added
                AddAugment(&augments, augment.GrantAugment());
            }
            // add a granted augment if required (only granted while augment selected)
            if (augment.HasGrantConditionalAugment())
            {
                if (augment.HasWeaponClass()
                    && m_item.HasWeapon())
                {
                    std::string weaponClass = EnumEntryText(augment.WeaponClass(), weaponClassTypeMap);
                    if (m_pBuild->IsWeaponInGroup(weaponClass, m_item.Weapon()))
                    {
                        // this is a new augment slot that needs to be added
                        AddAugment(&augments, augment.GrantConditionalAugment());
                    }
                }
            }
            if (!augments[augmentIndex].HasSelectedLevelIndex())
            {
                // default to the highest selectable option if not selected
                const std::vector<int>& levels = augment.Levels();
                size_t index = 0;
                for (size_t i = 0; i < levels.size(); ++i)
                {
                    if (levels[i] < static_cast<int>(m_pBuild->Level()))
                    {
                        index = i;
                    }
                }
                augments[augmentIndex].SetSelectedLevelIndex(index);
            }
            PopulateAugmentList(
                &m_comboAugmentDropList[augmentIndex],
                &m_augmentValues[augmentIndex],
                &m_augmentValues2[augmentIndex],
                &m_comboAugmentLevel[augmentIndex],
                augments[augmentIndex]);
        }
        m_item.Set_Augments(augments);
        // update the controls after a selection as edit controls may need
        // to be displayed for some augment selection controls.
        EnableControls();
    }
    HideTip();
}

void CFindGearDialog::OnAugmentCancel(UINT nID)
{
    UNREFERENCED_PARAMETER(nID);
    HideTip();
    m_bIgnoreNextMessage = true;
}

void CFindGearDialog::OnAugmentLevelSelect(UINT nID)
{
    int augmentIndex = nID - IDC_COMBO_LEVELAUGMENT1;
    ASSERT(augmentIndex >= 0 && augmentIndex < (int)m_item.Augments().size());
    int sel = m_comboAugmentLevel[augmentIndex].GetCurSel();
    if (sel != CB_ERR)
    {
        std::vector<ItemAugment> augments = m_item.Augments();
        augments[augmentIndex].SetSelectedLevelIndex(sel);
        m_item.Set_Augments(augments);
        PopulateAugmentList(
            &m_comboAugmentDropList[augmentIndex],
            &m_augmentValues[augmentIndex],
            &m_augmentValues2[augmentIndex],
            &m_comboAugmentLevel[augmentIndex],
            augments[augmentIndex]);
    }
}

void CFindGearDialog::OnAugmentLevelCancel(UINT nID)
{
    UNREFERENCED_PARAMETER(nID);
}

void CFindGearDialog::PopulateSlotUpgradeList(
        size_t controlIndex,
        const SlotUpgrade & upgrade)
{
    // set the text of the display item
    m_upgradeType[controlIndex].SetWindowText(upgrade.Type().c_str());
    // add the entries to the drop list control
    m_comboUpgradeDropList[controlIndex].ResetContent();
    std::vector<std::string> types = upgrade.UpgradeType();
    std::vector<std::string>::const_iterator it = types.begin();
    size_t index = 0;
    while (it != types.end())
    {
        size_t pos = m_comboUpgradeDropList[controlIndex].AddString((*it).c_str());
        m_comboUpgradeDropList[controlIndex].SetItemData(pos, index);
        // move on to the next item
        ++it;
        ++index;
    }
    // finally ensure the control is displayed
    m_upgradeType[controlIndex].ShowWindow(SW_SHOW);
    m_comboUpgradeDropList[controlIndex].ShowWindow(SW_SHOW);
}

void CFindGearDialog::OnUpgradeSelect(UINT nID)
{
    // an upgrade selection has been made. Determine which augment slot type
    // needs to be added and remove the upgrade list from the item. This action
    // cannot be undone. The user will have to reselect the base item to undo this
    size_t controlIndex = nID - IDC_COMBO_UPGRADE1;
    int selection = m_comboUpgradeDropList[controlIndex].GetCurSel();
    if (selection != CB_ERR)
    {
        selection = m_comboUpgradeDropList[controlIndex].GetItemData(selection);
        // now find what was selected
        std::vector<SlotUpgrade> upgrades = m_item.SlotUpgrades();
        std::vector<SlotUpgrade>::iterator sit = upgrades.begin();
        std::advance(sit, controlIndex);

        // determine the augment type to add
        std::vector<std::string> augments = (*sit).UpgradeType();
        ASSERT(selection >= 0 && selection < (int)augments.size());
        std::vector<std::string>::iterator it = augments.begin();
        std::advance(it, selection);

        // add the selected augment type to the item
        std::vector<ItemAugment> currentAugments = m_item.Augments();
        AddAugment(&currentAugments, (*it));
        m_item.Set_Augments(currentAugments);

        // now erase the upgrade from the upgrade list as its been actioned
        upgrades.erase(sit);
        m_item.Set_SlotUpgrades(upgrades);

        // now get all controls re-repopulate
        EnableControls();
    }
    HideTip();
}

void CFindGearDialog::OnUpgradeCancel(UINT nID)
{
    UNREFERENCED_PARAMETER(nID);
    HideTip();
    m_bIgnoreNextMessage = true;
}

void CFindGearDialog::OnKillFocusAugmentEdit(UINT nID)
{
    // user may have completed editing an edit field for a selectable augment
    // read the value and update if its changed
    CString text;

    size_t augmentIndex = 0;
    if (nID >= IDC_EDIT_AUGMENT1 && nID <= IDC_EDIT_AUGMENT10)
    {
        augmentIndex = nID - IDC_EDIT_AUGMENT1;
    }
    if (nID >= IDC_EDIT2_AUGMENT1 && nID <= IDC_EDIT2_AUGMENT10)
    {
        augmentIndex = nID - IDC_EDIT2_AUGMENT1;
    }
    m_augmentValues[augmentIndex].GetWindowText(text);
    double value1 = atof(text);
    std::vector<ItemAugment> augments = m_item.Augments();
    augments[augmentIndex].Set_Value(value1);
    if (m_augmentValues2[augmentIndex].IsWindowVisible())
    {
        m_augmentValues2[augmentIndex].GetWindowText(text);
        double value2 = atof(text);
        augments[augmentIndex].Set_Value2(value2);
    }
    m_item.Set_Augments(augments);
    EnableControls();
}

void CFindGearDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    if (IsWindow(m_editSearchText.GetSafeHwnd()))
    {
        // assign half the space to each set of controls:
        // +----------------------++-------------------+
        // | Item select control  || Augments section  |
        // |                      ||                   |
        // +----------------------++-------------------+
        //                         [OK] [CANCEL]

        // TBD!
    }
    if (IsWindow(m_editSearchText.GetSafeHwnd()))
    {
        // update the mouse hook handles for tooltips
        CRect rect;
        for (size_t i = 0 ; i < MAX_Augments; ++i)
        {
            m_comboAugmentDropList[i].GetWindowRect(&rect);
            rect.DeflateRect(0, 1, 0, 1);   // ensure they do not overlap
            GetMouseHook()->UpdateRectangle(
                    m_augmentHookHandles[i],
                    rect);          // screen coordinates
        }
    }
}

void CFindGearDialog::OnEndtrackListItems(NMHDR* pNMHDR, LRESULT* pResult)
{
    // just save the column widths to registry so restored next time we run
    UNREFERENCED_PARAMETER(pNMHDR);
    UNREFERENCED_PARAMETER(pResult);
    SaveColumnWidthsByName(&m_availableItemsCtrl, "FindGearDialog_%s");
}

void CFindGearDialog::OnColumnclickListItems(NMHDR* pNMHDR, LRESULT* pResult)
{
    // allow the user to sort the item list based on the selected column
    NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

    size_t columnToSort = pNMListView->iSubItem;
    int column;
    bool bAscending;
    m_sortHeader.GetSortArrow(&column, &bAscending);
    if ((size_t)column == columnToSort)
    {
        // just change sort direction
        bAscending = !bAscending;
    }
    else
    {
        // just change the column
        column = columnToSort;
    }
    m_sortHeader.SetSortArrow(column, bAscending);
    m_availableItemsCtrl.SortItems(CFindGearDialog::SortCompareFunction, (long)GetSafeHwnd());

    *pResult = 0;
}

int CFindGearDialog::SortCompareFunction(
        LPARAM lParam1,
        LPARAM lParam2,
        LPARAM lParamSort)
{
    // this is a static function so we need to make our own this pointer
    CWnd * pWnd = CWnd::FromHandle((HWND)lParamSort);
    CFindGearDialog * pThis = static_cast<CFindGearDialog*>(pWnd);

    int sortResult = 0;
    size_t index1 = lParam1; // item data index
    size_t index2 = lParam2; // item data index

    // need to find the actual current entry in the list control to compare the
    // text content
    index1 = FindItemIndexByItemData(&pThis->m_availableItemsCtrl, index1);
    index2 = FindItemIndexByItemData(&pThis->m_availableItemsCtrl, index2);

    // get the current sort settings
    int column;
    bool bAscending;
    pThis->m_sortHeader.GetSortArrow(&column, &bAscending);

    // get the control text entries for the column being sorted
    CString index1Text = pThis->m_availableItemsCtrl.GetItemText(index1, column);
    CString index2Text = pThis->m_availableItemsCtrl.GetItemText(index2, column);

    // some columns are converted to numeric to do the sort
    // while others are compared as ASCII
    switch (column)
    {
    case ILC_name:
        // ASCII sorts
        sortResult = (index1Text < index2Text) ? -1 : 1;
        break;
    case ILC_level:
        {
            // numeric sorts
            double val1 = atof(index1Text);
            double val2 = atof(index2Text);
            if (val1 == val2)
            {
                // if numeric match, sort by item name instead
                index1Text = pThis->m_availableItemsCtrl.GetItemText(index1, ILC_name);
                index2Text = pThis->m_availableItemsCtrl.GetItemText(index2, ILC_name);
                sortResult = (index1Text < index2Text) ? -1 : 1;
            }
            else
            {
                sortResult = (val1 < val2) ? -1 : 1;
            }
        }
        break;
    }
    if (!bAscending)
    {
        // switch sort direction result
        sortResult = -sortResult;
    }
    return sortResult;
}

void CFindGearDialog::OnHoverListItems(NMHDR* pNMHDR, LRESULT* pResult)
{
    UNREFERENCED_PARAMETER(pNMHDR);
    // the user it hovering over a list control item. Identify it and display
    // the item tooltip for this item
    CPoint mousePosition;
    GetCursorPos(&mousePosition);
    m_availableItemsCtrl.ScreenToClient(&mousePosition);

    LVHITTESTINFO hitInfo;
    hitInfo.pt = mousePosition;
    if (m_availableItemsCtrl.SubItemHitTest(&hitInfo) >= 0)
    {
        if (m_hoverItem != hitInfo.iItem)
        {
            // the item under the hover has changed
            m_hoverItem = hitInfo.iItem;
            // mouse is over a valid item, get the items rectangle and
            // show the item tooltip
            CRect rect;
            m_availableItemsCtrl.GetItemRect(hitInfo.iItem, &rect, LVIR_BOUNDS);
            HideTip();
            // get the item index
            size_t itemIndex = m_availableItemsCtrl.GetItemData(hitInfo.iItem);
            m_availableItemsCtrl.ClientToScreen(&rect);
            CPoint tipTopLeft(rect.left, rect.bottom);
            CPoint tipAlternate(rect.left, rect.top);
            std::list<const Item*>::const_iterator it = m_availableItems.begin();
            std::advance(it, itemIndex);
            SetTooltipText(*(*it), tipTopLeft, tipAlternate);
            m_showingTip = true;
            // make sure we don't stack multiple monitoring of the same rectangle
            if (m_hoverHandle == 0)
            {
                m_hoverHandle = GetMouseHook()->AddRectangleToMonitor(
                        this->GetSafeHwnd(),
                        rect,           // screen coordinates,
                        WM_MOUSEENTER,
                        WM_MOUSELEAVE,
                        true);
            }
        }
    }
    *pResult = 1;   // stop the hover selecting the item
}

LRESULT CFindGearDialog::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    // hide any tooltip when the mouse leaves the area its being shown for
    HideTip();
    if (wParam == m_hoverHandle)
    {
        // no longer monitoring this location
        GetMouseHook()->DeleteRectangleToMonitor(m_hoverHandle);
        m_hoverHandle = 0;
        m_hoverItem = -1;
    }
    return 0;
}

void CFindGearDialog::ShowTip(const Item & item, CRect itemRect)
{
    if (m_showingTip)
    {
        m_tooltip.Hide();
    }
    ClientToScreen(&itemRect);
    CPoint tipTopLeft(itemRect.left, itemRect.bottom + 2);
    CPoint tipAlternate(itemRect.left, itemRect.top - 2);
    SetTooltipText(item, tipTopLeft, tipAlternate);
    m_showingTip = true;
}

void CFindGearDialog::HideTip()
{
    // tip not shown if not over a gear slot
    if (m_tipCreated && m_showingTip)
    {
        m_tooltip.Hide();
        m_showingTip = false;
    }
}

void CFindGearDialog::SetTooltipText(
        const Item & item,
        CPoint tipTopLeft,
        CPoint tipAlternate)
{
    m_tooltip.SetOrigin(tipTopLeft, tipAlternate, false);
    m_tooltip.SetItem(&item, m_pBuild);
    m_tooltip.Show();
    m_showingTip = true;
}

void CFindGearDialog::SetTooltipText(
        const Augment & augment,
        CPoint tipTopLeft,
        CPoint tipAlternate,
        bool rightAlign)
{
    m_tooltip.SetOrigin(tipTopLeft, tipAlternate, rightAlign);
    m_tooltip.SetAugment(&augment);
    m_tooltip.Show();
    m_showingTip = true;
}

LRESULT CFindGearDialog::OnHoverComboBox(WPARAM wParam, LPARAM lParam)
{
    if (!m_bIgnoreNextMessage)
    {
        // wParam = selected index
        // lParam = control ID
        // as these are all augment combo boxes, we can treat them all the same
        if (m_showingTip)
        {
            m_tooltip.Hide();
        }
        if (wParam >= 0)
        {
            // we have a selection, get the filigree name
            CString augmentName;
            CWnd * pWnd = GetDlgItem(lParam);
            CComboBox * pCombo =  dynamic_cast<CComboBox*>(pWnd);
            pCombo->GetLBText(wParam, augmentName);
            if (!augmentName.IsEmpty())
            {
                CRect rctWindow;
                pCombo->GetWindowRect(&rctWindow);
                rctWindow.right = rctWindow.left + pCombo->GetDroppedWidth();
                // tip is shown to the left or the right of the combo box
                CPoint tipTopLeft(rctWindow.left, rctWindow.top);
                CPoint tipAlternate(rctWindow.right, rctWindow.top);
                Augment augment = FindAugmentByName((LPCTSTR)augmentName, &m_item);
                SetTooltipText(augment, tipTopLeft, tipAlternate, true);
                m_showingTip = true;
            }
        }
    }
    m_bIgnoreNextMessage = false;
    return 0;
}

LRESULT CFindGearDialog::OnMouseEnter(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    // is it a jewel or a filigree?
    for (size_t i = 0 ; i < MAX_Augments; ++i)
    {
        if ((int)wParam == m_augmentHookHandles[i])
        {
            int sel = m_comboAugmentDropList[i].GetCurSel();
            if (sel != CB_ERR)
            {
                CString augmentName;
                m_comboAugmentDropList[i].GetLBText(
                        sel,
                        augmentName);
                if (!augmentName.IsEmpty())
                {
                    CRect itemRect;
                    m_comboAugmentDropList[i].GetWindowRect(&itemRect);
                    Augment augment = FindAugmentByName((LPCTSTR)augmentName, &m_item);
                    CPoint tipTopLeft(itemRect.left, itemRect.bottom + 2);
                    CPoint tipAlternate(itemRect.left, itemRect.top - 2);
                    SetTooltipText(augment, tipTopLeft, tipAlternate, false);
                    break;
                }
            }
        }
    }
    return 0;
}

void CFindGearDialog::OnWindowPosChanging(WINDOWPOS * pos)
{
    // ensure tooltip locations are correct on window move
    CDialog::OnWindowPosChanging(pos);
    PostMessage(WM_SIZE, SIZE_RESTORED, MAKELONG(pos->cx, pos->cy));
}

void CFindGearDialog::AddAugment(
        std::vector<ItemAugment> * augments,
        const std::string & name,
        bool atEnd)
{
    // only add if it is not already present
    bool found = false;
    for (size_t i = 0; i < augments->size(); ++i)
    {
        if ((*augments)[i].Type() == name)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        // this is a new augment slot that needs to be added
        ItemAugment newAugment;
        newAugment.Set_Type(name);
        // all new augments are added before any "Mythic" slot if present
        bool foundMythic = false;
        size_t i;
        for (i = 0; i < augments->size(); ++i)
        {
            if ((*augments)[i].Type() == "Mythic")
            {
                foundMythic = true;
                break;
            }
        }
        if (!atEnd && foundMythic)
        {
            std::vector<ItemAugment>::iterator it = augments->begin();
            std::advance(it, i);
            augments->insert(it, newAugment);
        }
        else
        {
            // just add to the end if no current Mythic slot
            augments->push_back(newAugment);
        }
    }
}

void CFindGearDialog::RemoveAugment(
        std::vector<ItemAugment> * augments,
        const std::string & name)
{
    for (size_t i = 0; i < augments->size(); ++i)
    {
        if ((*augments)[i].Type() == name)
        {
            // this one needs removing
            std::vector<ItemAugment>::iterator it = augments->begin();
            std::advance(it, i);
            augments->erase(it);
            // we're done
            break;
        }
    }
}

void CFindGearDialog::OnSearchTextKillFocus()
{
    CString text;
    m_editSearchText.GetWindowText(text);
    if (text != m_previousSearchText)
    {
        // just update the list of items as the search text changes
        m_bInitialising = true;
        PopulateAvailableItemList();
        m_bInitialising = false;
        m_previousSearchText = text;
    }
}

BOOL CFindGearDialog::PreTranslateMessage(MSG* pMsg)
{
    BOOL handled = FALSE;
    // if the user presses return/enter on text in the filter control,
    // we just set the focus to the filter button
    if (pMsg->message == WM_KEYDOWN
            && pMsg->wParam == VK_RETURN
            && GetFocus() == &m_editSearchText)
    {
        m_availableItemsCtrl.SetFocus();
        handled = TRUE;
    }
    if (!handled)
    {
        handled = CDialog::PreTranslateMessage(pMsg);
    }
    return handled;
}

void CFindGearDialog::OnButtonEquipIt()
{
    std::vector<ItemAugment> augments = m_item.Augments();
    for (auto&& ait : augments)
    {
        if (ait.Type().find("Cannith") != std::string::npos)
        {
            ait.SetSelectedLevelIndex(m_item.MinLevel());
        }
    }
    InventorySlotType slot = Inventory_Unknown;
    // determine which slot(s) this item can equip to
    std::vector<InventorySlotType> slots;
    for (size_t i = Inventory_Unknown; i < Inventory_Count; ++i)
    {
        if (m_item.CanEquipToSlot((InventorySlotType)(i)))
        {
            slots.push_back((InventorySlotType)(i));
        }
    }
    // see which slot to equip to
    if (slots.size() > 0)
    {
        if (slots.size() > 1)
        {
            // let the user choose which slot this item is to go in as
            // it can go into 2 or more positions
            slot = SelectTargetSlot(slots);
        }
        else
        {
            // single slot, must go in that one
            slot = slots[0];
        }
        if (slot != Inventory_Unknown)
        {
            m_pBuild->SetGear(
                    m_pBuild->ActiveGearSet().Name(),
                    slot,
                    m_item);
        }
    }
}

InventorySlotType CFindGearDialog::SelectTargetSlot(const std::vector<InventorySlotType> & slots)
{
    // build a menu of the available slot names and let user select it
    CMenu cMenu;
    cMenu.CreatePopupMenu();
    cMenu.AppendMenu(
            MF_STRING | MF_GRAYED,
            0,
            "Select equipment slot to populate");
    cMenu.AppendMenu(MF_SEPARATOR);
    for (size_t i = 0; i < slots.size(); ++i)
    {
        CString itemText;
        itemText = (LPCTSTR)EnumEntryText(slots[i], InventorySlotTypeMap);
        cMenu.AppendMenu(
                MF_STRING,
                ID_INVENTORY_UNKNOWN + slots[i],
                itemText);
    }
    CPoint p;
    GetCursorPos(&p);
    CWinAppEx * pApp = dynamic_cast<CWinAppEx*>(AfxGetApp());
    UINT sel = pApp->GetContextMenuManager()->TrackPopupMenu(
            cMenu.GetSafeHmenu(),
            p.x,
            p.y,
            this);
    InventorySlotType slot = Inventory_Unknown;
    if (sel >= ID_INVENTORY_UNKNOWN)
    {
        slot = (InventorySlotType)(sel - ID_INVENTORY_UNKNOWN);
    }
    return slot;
}

void CFindGearDialog::OnItemLevelSelect()
{
    if (m_item.HasUserSetsLevel())
    {
        int sel = m_comboItemLevel.GetCurSel();
        if (sel != CB_ERR)
        {
            m_item.Set_MinLevel(sel + 1);       // 1 based
            EnableControls();
        }
    }
}

void CFindGearDialog::OnSelEndLevelRange()
{
    int sel = m_comboLevelRange.GetCurSel();
    if (sel != CB_ERR)
    {
        CString text;
        m_comboLevelRange.GetLBText(sel, text);
        m_levelRange = atoi(text);
        PopulateAvailableItemList();
    }
}

void CFindGearDialog::SetupItemLevel(size_t maxLevel, size_t currentLevel, bool bSelectable)
{
    m_comboItemLevel.LockWindowUpdate();
    m_comboItemLevel.ResetContent();
    for (size_t level = 0; level < maxLevel; ++level)
    {
        CString txt;
        txt.Format("%d", level + 1);
        m_comboItemLevel.AddString(txt);
    }
    if (currentLevel > 0)
    {
        // normal items are 1 base, convert to 0 based
        --currentLevel;
    }
    else
    {
        // on selection cannith crafted defaults to level 1 from 0
        m_item.Set_MinLevel(1);
    }
    m_comboItemLevel.SetCurSel(currentLevel);
    m_comboItemLevel.EnableWindow(bSelectable);
    m_comboItemLevel.UnlockWindowUpdate();
}

void CFindGearDialog::OnButtonIgnoreRaidItems()
{
    int ignoreRaidItems = (m_buttonIgnoreRaidItems.GetCheck() == BST_CHECKED) ? 1 : 0;
    AfxGetApp()->WriteProfileInt("ItemSelectDialog", "ExcludeRaidItems", ignoreRaidItems);
    PopulateAvailableItemList();
}

void CFindGearDialog::OnButtonClearFilter()
{
    m_editSearchText.SetWindowText("");
    OnSearchTextKillFocus();
}
