// QArchiver / 小Q解压
// Explorer context-menu extension: dynamic "添加到 <name>.zip" labels and a
// "压缩到..." format submenu.  Implemented as a small in-process COM server so
// the menu text can include the selected file name and archive files can be
// excluded dynamically.
// 资源管理器右键菜单扩展：动态“添加到 <名称>.zip”文案与“压缩到...”格式子菜单。
// 以进程内 COM 服务器实现，因此菜单文案可以包含所选文件名，并能动态排除
// 已经是压缩包的文件。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <new>
#include <string>
#include <vector>

namespace {

const CLSID kClsid = {0x5f3b9c2a, 0x7e4d, 0x4a1b,
                      {0x9c, 0x6d, 0x2b, 0x8e, 0x1f, 0x4a, 0x7c, 0x30}};
const wchar_t *kClsidString = L"{5F3B9C2A-7E4D-4A1B-9C6D-2B8E1F4A7C30}";
const wchar_t *kHandlerName = L"QArchiver";
const wchar_t *kApprovedName = L"QArchiver (小Q解压)";

HINSTANCE g_instance = nullptr;
volatile LONG g_objectCount = 0;

std::wstring modulePath()
{
    wchar_t buffer[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameW(g_instance, buffer, _countof(buffer));
    if (length == 0 || length >= _countof(buffer)) {
        return std::wstring();
    }
    return std::wstring(buffer, length);
}

std::wstring executablePath()
{
    std::wstring path = modulePath();
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.erase(slash + 1);
    }
    path += L"QArchiver.exe";
    return path;
}

std::wstring quote(const std::wstring &value)
{
    return L"\"" + value + L"\"";
}

bool hasArchiveSuffix(const std::wstring &path)
{
    static const wchar_t *suffixes[] = {
        L".zip", L".7z", L".rar", L".tar", L".gz", L".bz2", L".xz",
        L".tgz", L".txz", L".tbz2", L".tar.gz", L".tar.xz", L".tar.bz2",
        L".tar.zst", L".zst", L".cab", L".iso"
    };
    std::wstring lower = path;
    CharLowerBuffW(lower.data(), static_cast<DWORD>(lower.size()));
    for (const wchar_t *suffix : suffixes) {
        const size_t length = wcslen(suffix);
        if (lower.size() >= length
            && lower.compare(lower.size() - length, length, suffix) == 0) {
            return true;
        }
    }
    return false;
}

std::wstring fileName(const std::wstring &path)
{
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring baseName(const std::wstring &path, bool isDirectory)
{
    std::wstring name = fileName(path);
    if (isDirectory) {
        return name.empty() ? L"archive" : name;
    }
    // Remove compound archive suffixes first, then one extension.
    std::wstring lower = name;
    CharLowerBuffW(lower.data(), static_cast<DWORD>(lower.size()));
    static const wchar_t *compound[] = {L".tar.gz", L".tar.xz", L".tar.bz2",
                                        L".tar.zst"};
    for (const wchar_t *suffix : compound) {
        const size_t length = wcslen(suffix);
        if (lower.size() > length
            && lower.compare(lower.size() - length, length, suffix) == 0) {
            name.erase(name.size() - length);
            lower.erase(lower.size() - length);
            break;
        }
    }
    const size_t dot = lower.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0) {
        name.erase(dot);
    }
    return name.empty() ? L"archive" : name;
}

bool launch(const std::wstring &arguments)
{
    const std::wstring exe = executablePath();
    if (exe.empty() || !PathFileExistsW(exe.c_str())) {
        return false;
    }
    std::wstring commandLine = quote(exe) + L" " + arguments;
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL ok = CreateProcessW(
        nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startup, &process);
    if (ok) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    return ok == TRUE;
}

void runCompression(const std::vector<std::wstring> &paths,
                    const wchar_t *format, bool customDialog)
{
    if (paths.empty()) {
        return;
    }
    if (customDialog) {
        std::wstring arguments = L"--compress-dialog";
        for (const std::wstring &path : paths) {
            arguments += L" " + quote(path);
        }
        launch(arguments);
        return;
    }
    // One process per item keeps the "name.zip next to each file" semantics
    // for multi-selection.
    // 多选时每项单独启动一个进程，保持“每个文件旁生成同名 zip”的语义。
    for (const std::wstring &path : paths) {
        launch(std::wstring(L"--compress ") + format + L" " + quote(path));
    }
}

class ContextMenu final : public IShellExtInit, public IContextMenu {
public:
    ContextMenu() { InterlockedIncrement(&g_objectCount); }
    ~ContextMenu() { InterlockedDecrement(&g_objectCount); }

    // IUnknown / IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
    {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (riid == IID_IUnknown || riid == IID_IShellExtInit) {
            *object = static_cast<IShellExtInit *>(this);
        } else if (riid == IID_IContextMenu) {
            *object = static_cast<IContextMenu *>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&m_reference));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG reference = InterlockedDecrement(&m_reference);
        if (reference == 0) {
            delete this;
        }
        return static_cast<ULONG>(reference);
    }

    // IShellExtInit / IShellExtInit
    HRESULT STDMETHODCALLTYPE Initialize(PCIDLIST_ABSOLUTE folderPidl,
                                         IDataObject *dataObject,
                                         HKEY) override
    {
        m_paths.clear();
        m_folder.clear();
        m_background = false;

        if (folderPidl) {
            wchar_t folder[MAX_PATH * 4] = {};
            if (SHGetPathFromIDListW(folderPidl, folder)) {
                m_folder = folder;
            }
        }
        if (!dataObject) {
            return E_INVALIDARG;
        }

        FORMATETC format = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1,
                            TYMED_HGLOBAL};
        STGMEDIUM medium = {};
        if (SUCCEEDED(dataObject->GetData(&format, &medium))) {
            if (HDROP drop = static_cast<HDROP>(GlobalLock(medium.hGlobal))) {
                const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; ++i) {
                    wchar_t path[MAX_PATH * 4] = {};
                    if (DragQueryFileW(drop, i, path, _countof(path)) > 0) {
                        m_paths.emplace_back(path);
                    }
                }
                GlobalUnlock(medium.hGlobal);
            }
            ReleaseStgMedium(&medium);
        }

        m_background = m_paths.empty() && !m_folder.empty();
        return S_OK;
    }

    // IContextMenu / IContextMenu
    HRESULT STDMETHODCALLTYPE QueryContextMenu(HMENU menu, UINT index,
                                               UINT firstCommand,
                                               UINT lastCommand,
                                               UINT flags) override
    {
        if (flags & CMF_DEFAULTONLY) {
            return 0;
        }
        std::vector<std::wstring> targets = m_paths;
        if (targets.empty() && m_background && !m_folder.empty()) {
            targets.push_back(m_folder);
        }
        if (targets.empty()) {
            return 0;
        }
        // Archive files already are compressed: no "add to archive" menu.
        // 压缩包本身不再显示“添加到压缩包”。
        const DWORD attributes = GetFileAttributesW(targets.front().c_str());
        const bool isDirectory =
            attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!isDirectory && hasArchiveSuffix(targets.front())) {
            return 0;
        }
        if (lastCommand < firstCommand + 8) {
            return 0;
        }

        const std::wstring zipName =
            baseName(targets.front(), isDirectory) + L".zip";
        const std::wstring label = L"添加到 " + quote(zipName);
        InsertMenuW(menu, index, MF_BYPOSITION | MF_STRING, firstCommand,
                    label.c_str());

        HMENU submenu = CreatePopupMenu();
        AppendMenuW(submenu, MF_STRING, firstCommand + 1, L"7z 压缩包 (.7z)");
        AppendMenuW(submenu, MF_STRING, firstCommand + 2, L"ZIP 压缩包 (.zip)");
        AppendMenuW(submenu, MF_STRING, firstCommand + 3, L"Gzip tar (.tar.gz)");
        AppendMenuW(submenu, MF_STRING, firstCommand + 4, L"Xz tar (.tar.xz)");
        AppendMenuW(submenu, MF_STRING, firstCommand + 5,
                    L"Bzip2 tar (.tar.bz2)");
        AppendMenuW(submenu, MF_STRING, firstCommand + 6, L"Tar (.tar)");
        AppendMenuW(submenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(submenu, MF_STRING, firstCommand + 7,
                    L"自定义压缩选项...");
        InsertMenuW(menu, index + 1, MF_BYPOSITION | MF_POPUP,
                    reinterpret_cast<UINT_PTR>(submenu), L"压缩到...");

        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 8);
    }

    HRESULT STDMETHODCALLTYPE InvokeCommand(LPCMINVOKECOMMANDINFO info) override
    {
        if (!info || HIWORD(info->lpVerb) != 0) {
            return E_INVALIDARG;
        }
        std::vector<std::wstring> targets = m_paths;
        if (targets.empty() && m_background && !m_folder.empty()) {
            targets.push_back(m_folder);
        }
        if (targets.empty()) {
            return E_INVALIDARG;
        }

        switch (LOWORD(info->lpVerb)) {
        case 0: runCompression(targets, L"zip", false); break;
        case 1: runCompression(targets, L"7z", false); break;
        case 2: runCompression(targets, L"zip", false); break;
        case 3: runCompression(targets, L"tar.gz", false); break;
        case 4: runCompression(targets, L"tar.xz", false); break;
        case 5: runCompression(targets, L"tar.bz2", false); break;
        case 6: runCompression(targets, L"tar", false); break;
        case 7: runCompression(targets, L"", true); break;
        default: return E_INVALIDARG;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCommandString(UINT_PTR, UINT, UINT *,
                                               CHAR *, UINT) override
    {
        return E_NOTIMPL;
    }

private:
    LONG m_reference = 1;
    std::vector<std::wstring> m_paths;
    std::wstring m_folder;
    bool m_background = false;
};

class ClassFactory final : public IClassFactory {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
    {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *object = static_cast<IClassFactory *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&m_reference));
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG reference = InterlockedDecrement(&m_reference);
        if (reference == 0) {
            delete this;
        }
        return static_cast<ULONG>(reference);
    }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID riid,
                                             void **object) override
    {
        if (outer) {
            return CLASS_E_NOAGGREGATION;
        }
        auto *menu = new (std::nothrow) ContextMenu();
        if (!menu) {
            return E_OUTOFMEMORY;
        }
        const HRESULT result = menu->QueryInterface(riid, object);
        menu->Release();
        return result;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override
    {
        if (lock) {
            InterlockedIncrement(&g_objectCount);
        } else {
            InterlockedDecrement(&g_objectCount);
        }
        return S_OK;
    }

private:
    LONG m_reference = 1;
};

// ---------------------------------------------------------------------------
// Registration helpers / 注册辅助
// ---------------------------------------------------------------------------

HKEY classesRoot()
{
    return IsUserAnAdmin() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

bool setString(HKEY root, const std::wstring &key, const wchar_t *name,
               const std::wstring &value)
{
    HKEY handle = nullptr;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr,
                        &handle, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const DWORD bytes =
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LONG result = RegSetValueExW(
        handle, name, 0, REG_SZ,
        reinterpret_cast<const BYTE *>(value.c_str()), bytes);
    RegCloseKey(handle);
    return result == ERROR_SUCCESS;
}

bool deleteTree(HKEY root, const std::wstring &key)
{
    const LONG result = RegDeleteTreeW(root, key.c_str());
    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

void registerHandler(HKEY root, const std::wstring &base)
{
    setString(root, base + L"\\shellex\\ContextMenuHandlers\\" + kHandlerName,
              nullptr, kClsidString);
}

void unregisterHandler(HKEY root, const std::wstring &base)
{
    deleteTree(root,
               base + L"\\shellex\\ContextMenuHandlers\\" + kHandlerName);
}

void notifyShell()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

} // namespace

// ---------------------------------------------------------------------------
// COM exports / COM 导出
// ---------------------------------------------------------------------------

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **object)
{
    if (!IsEqualCLSID(clsid, kClsid)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    auto *factory = new (std::nothrow) ClassFactory();
    if (!factory) {
        return E_OUTOFMEMORY;
    }
    const HRESULT result = factory->QueryInterface(riid, object);
    factory->Release();
    return result;
}

STDAPI DllCanUnloadNow()
{
    return g_objectCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    const std::wstring dll = modulePath();
    if (dll.empty()) {
        return E_FAIL;
    }
    HKEY root = classesRoot();
    const std::wstring clsidKey =
        std::wstring(L"Software\\Classes\\CLSID\\") + kClsidString;
    if (!setString(root, clsidKey, nullptr, kApprovedName)
        || !setString(root, clsidKey + L"\\InprocServer32", nullptr, dll)
        || !setString(root, clsidKey + L"\\InprocServer32",
                      L"ThreadingModel", L"Apartment")) {
        return E_FAIL;
    }
    registerHandler(root, L"Software\\Classes\\*");
    registerHandler(root, L"Software\\Classes\\Directory");
    registerHandler(root, L"Software\\Classes\\Directory\\Background");

    // Best effort: the Approved list is not required by modern Explorer but
    // some older shells use it to filter extensions.
    // 尽力写入 Approved 列表；现代资源管理器不强制要求。
    setString(HKEY_LOCAL_MACHINE,
              L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell "
              L"Extensions\\Approved",
              kClsidString, kApprovedName);
    notifyShell();
    return S_OK;
}

STDAPI DllUnregisterServer()
{
    HKEY root = classesRoot();
    unregisterHandler(root, L"Software\\Classes\\*");
    unregisterHandler(root, L"Software\\Classes\\Directory");
    unregisterHandler(root, L"Software\\Classes\\Directory\\Background");
    const std::wstring clsidKey =
        std::wstring(L"Software\\Classes\\CLSID\\") + kClsidString;
    deleteTree(root, clsidKey);
    RegDeleteKeyValueW(HKEY_LOCAL_MACHINE,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell "
                       L"Extensions\\Approved",
                       kClsidString);
    notifyShell();
    return S_OK;
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
