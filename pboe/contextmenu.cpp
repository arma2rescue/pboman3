#include "contextmenu.h"
#include <Shlwapi.h>
#include <cassert>
#include "dllmain.h"
#include <filesystem>
#include "registry.h"
#include <wingdi.h>
#include <strsafe.h>

// ReSharper disable CppZeroConstantCanBeReplacedWithNullptr

namespace pboman3 {
    ContextMenu::ContextMenu()
        : refCount_(1),
          subMenu_(NULL),
          icon_(NULL),
          selectedPaths_(nullptr),
          executable_(nullptr) {
        DllAddRef();
    }

    ContextMenu::~ContextMenu() {
        DllRelease();
        if (subMenu_)
            DestroyMenu(subMenu_);
        if (icon_)
            DeleteObject(icon_);
    }

    HRESULT ContextMenu::QueryInterface(const IID& riid, void** ppvObject) {
        const QITAB qit[] = {
            QITABENT(ContextMenu, IContextMenu),
            QITABENT(ContextMenu, IShellExtInit),
            {0, 0}
        };
        return QISearch(this, qit, riid, ppvObject);
    }

    ULONG ContextMenu::AddRef() {
        return InterlockedIncrement(&refCount_);
    }

    ULONG ContextMenu::Release() {
        const ULONG result = InterlockedDecrement(&refCount_);
        if (result == 0)
            delete this;
        return result;
    }

    HRESULT ContextMenu::Initialize(LPCITEMIDLIST pidlFolder, IDataObject* pdtobj, HKEY hkeyProgId) {
        icon_ = loadRootIcon();
        selectedPaths_ = getSelectedPaths(pdtobj);
        executable_ = Executable::fromRegistry();
        return S_OK;
    }

    constexpr int idUnpackFilePt = 1;
    constexpr int idUnpackFileAs = 2;
    constexpr int idUnpackMultiPt = 3;
    constexpr int idUnpackMultiIn = 4;
    constexpr int idPackFilePt = 5;
    constexpr int idPackFileAs = 6;
    constexpr int idPackMultiPt = 7;
    constexpr int idPackMultiIn = 8;

    HRESULT ContextMenu::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags) {
        HRESULT hr = E_FAIL;
        if (executable_->isValid()) {
            const SelectionMode sel = getSelectionMode();
            UINT lastUsedMenuIndex;

            if (sel == SelectionMode::Files) {
                subMenu_ = CreateMenu();

                if (selectedPaths_->size() == 1) {
                    TCHAR textItem1[] = "Unpack to...";
                    const MENUITEMINFO item1 = makeMenuItem(idCmdFirst + idUnpackFilePt, textItem1);
                    InsertMenuItem(subMenu_, 0, TRUE, &item1);

                    string textItem2 = "Unpack as \"" + selectedPaths_->at(0)
                                                                      .filename().replace_extension().string() + "/\"";
                    const MENUITEMINFO item2 = makeMenuItem(idCmdFirst + idUnpackFileAs, textItem2.data());
                    InsertMenuItem(subMenu_, 0, TRUE, &item2);

                    lastUsedMenuIndex = idUnpackFileAs;
                } else {
                    TCHAR textItem1[] = "Unpack to...";
                    const MENUITEMINFO item3 = makeMenuItem(idCmdFirst + idUnpackMultiPt, textItem1);
                    InsertMenuItem(subMenu_, 0, TRUE, &item3);

                    string textItem2 = "Unpack in \"" + selectedPaths_->at(0).parent_path().filename().string() + "/\"";
                    const MENUITEMINFO item4 = makeMenuItem(idCmdFirst + idUnpackMultiIn, textItem2.data());
                    InsertMenuItem(subMenu_, 0, TRUE, &item4);

                    lastUsedMenuIndex = idUnpackMultiIn;
                }

                insertRootItem(hmenu, indexMenu);
                hr = MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(lastUsedMenuIndex + 1));
            } else if (sel == SelectionMode::Folders) {
                subMenu_ = CreateMenu();

                if (selectedPaths_->size() == 1) {
                    TCHAR textItem1[] = "Pack to...";
                    const MENUITEMINFO item1 = makeMenuItem(idCmdFirst + idPackFilePt, textItem1);
                    InsertMenuItem(subMenu_, 0, TRUE, &item1);

                    string textItem2 = "Pack as \"" + selectedPaths_->at(0).filename().string() + ".pbo\"";
                    const MENUITEMINFO item2 = makeMenuItem(idCmdFirst + idPackFileAs, textItem2.data());
                    InsertMenuItem(subMenu_, 0, TRUE, &item2);

                    lastUsedMenuIndex = idPackFileAs;
                } else {
                    TCHAR textItem3[] = "Pack to...";
                    const MENUITEMINFO item1 = makeMenuItem(idCmdFirst + idPackMultiPt, textItem3);
                    InsertMenuItem(subMenu_, 0, TRUE, &item1);

                    string textItem4 = "Pack in \"" + selectedPaths_->at(0).parent_path().filename().string() + "/\"";
                    const MENUITEMINFO item2 = makeMenuItem(idCmdFirst + idPackMultiIn, textItem4.data());
                    InsertMenuItem(subMenu_, 0, TRUE, &item2);

                    lastUsedMenuIndex = idPackMultiIn;
                }

                insertRootItem(hmenu, indexMenu);
                hr = MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(lastUsedMenuIndex + 1));
            }
        }

        return hr;
    }

    HRESULT ContextMenu::InvokeCommand(CMINVOKECOMMANDINFO* pici) {
        //https://docs.microsoft.com/en-us/windows/win32/shell/how-to-implement-the-icontextmenu-interface

        HRESULT hr = E_FAIL;

        if (pici->cbSize == sizeof CMINVOKECOMMANDINFOEX
            && pici->fMask & CMIC_MASK_UNICODE) {
            const auto piciw = reinterpret_cast<CMINVOKECOMMANDINFOEX*>(pici);
            if (HIWORD(piciw->lpVerbW) == 0) {
                //means pici contains idCmd, otherwise would mean pici contains a verb
                const auto idCmd = LOWORD(pici->lpVerb);
                switch (idCmd) {
                    case idUnpackFilePt:
                    case idUnpackMultiPt:
                        if (executable_->unpackFiles(pici->lpDirectory, *selectedPaths_))
                            hr = S_OK;
                        break;
                    case idUnpackFileAs:
                    case idUnpackMultiIn:
                        if (executable_->unpackFiles(pici->lpDirectory, *selectedPaths_, pici->lpDirectory))
                            hr = S_OK;
                        break;
                    case idPackFilePt:
                    case idPackMultiPt:
                        break;
                    case idPackFileAs:
                    case idPackMultiIn:
                        break;
                }
            }
        }

        return hr;
    }

    HRESULT ContextMenu::GetCommandString(UINT_PTR idCmd, UINT uType, UINT* pReserved, CHAR* pszName, UINT cchMax) {
        return E_NOTIMPL;
    }

    shared_ptr<vector<path>> ContextMenu::getSelectedPaths(IDataObject* dataObject) const {
        auto res = make_shared<vector<path>>();
        FORMATETC fe = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM stg;
        if (SUCCEEDED(dataObject->GetData(&fe, &stg))) {
            const UINT fileCount = DragQueryFileA(static_cast<HDROP>(stg.hGlobal), 0xFFFFFFFF, NULL, 0);
            res->reserve(fileCount);
            for (UINT i = 0; i < fileCount; i++) {
                CHAR fileName[MAX_PATH];
                const UINT fileNameLen = DragQueryFileA(static_cast<HDROP>(stg.hGlobal), i, fileName, sizeof fileName);
                if (fileNameLen) {
                    const path p(string(fileName, fileNameLen));
                    if (is_directory(p) || is_regular_file(p))
                        res->emplace_back(p);
                }
            }

            ReleaseStgMedium(&stg);
        }
        return res;
    }

    MENUITEMINFO ContextMenu::makeMenuItem(UINT wId, LPTSTR text) const {
        MENUITEMINFO item;
        item.cbSize = sizeof item;
        item.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
        item.fType = MFT_STRING;
        item.fState = MFS_ENABLED;
        item.wID = wId;
        item.dwTypeData = text;
        item.cch = sizeof item.dwTypeData;
        // ReSharper disable once CppSomeObjectMembersMightNotBeInitialized
        return item;
    }

    void ContextMenu::insertRootItem(HMENU hmenu, UINT indexMenu) const {
        TCHAR menuText[] = "PBO Manager";
        const MENUITEMINFO menu = makeRootItem(menuText, icon_, subMenu_);
        InsertMenuItem(hmenu, indexMenu, TRUE, &menu);
    }

    MENUITEMINFO ContextMenu::makeRootItem(LPTSTR text, HBITMAP icon, HMENU subMenu) const {
        MENUITEMINFO menu;
        menu.cbSize = sizeof menu;
        menu.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_SUBMENU;
        menu.fType = MFT_STRING;
        menu.fState = MFS_ENABLED;
        menu.hSubMenu = subMenu;
        menu.dwTypeData = text;
        menu.cch = sizeof menu.dwTypeData;

        if (icon) {
            menu.fMask |= MIIM_BITMAP;
            menu.hbmpItem = icon;
        }
        // ReSharper disable once CppSomeObjectMembersMightNotBeInitialized
        return menu;
    }

    HBITMAP ContextMenu::loadRootIcon() const {
        const string exePath = Registry::getExecutablePath();
        if (exePath.empty() || !is_regular_file(exePath))
            return NULL;

        const int cx = GetSystemMetrics(SM_CXSMICON);
        const int cy = GetSystemMetrics(SM_CYSMICON);

        HICON icon;
        if (!ExtractIconEx(exePath.data(), 0, &icon, NULL, 1))
            return NULL;

        // ReSharper disable CppLocalVariableMayBeConst
        HDC hdc = CreateDC("DISPLAY", NULL, NULL, NULL);

        HDC dc = CreateCompatibleDC(hdc);

        HBITMAP bitmap = CreateCompatibleBitmap(hdc, cx, cy);
        HGDIOBJ prev = SelectObject(dc, bitmap);

        HBRUSH brush = GetSysColorBrush(COLOR_MENU);
        RECT rect{0, 0, cx, cy};
        FillRect(dc, &rect, brush);

        DrawIconEx(dc, 0, 0, icon, cx, cy, 0, NULL, DI_NORMAL);

        HGDIOBJ btmp = SelectObject(dc, prev);
        assert(btmp == bitmap);
        // ReSharper restore CppLocalVariableMayBeConst

        DestroyIcon(icon);
        DeleteDC(dc);
        DeleteDC(hdc);

        return bitmap;
    }

    ContextMenu::SelectionMode ContextMenu::getSelectionMode() const {
        auto res = SelectionMode::None;

        if (selectedPaths_ && !selectedPaths_->empty()) {
            for (const path& path : *selectedPaths_) {
                if (is_regular_file(path)) {
                    if (res == SelectionMode::Folders) {
                        res = SelectionMode::Mixed;
                        break;
                    }
                    res = SelectionMode::Files;
                } else {
                    if (res == SelectionMode::Files) {
                        res = SelectionMode::Mixed;
                        break;
                    }
                    res = SelectionMode::Folders;
                }
            }
        }

        return res;
    }
}