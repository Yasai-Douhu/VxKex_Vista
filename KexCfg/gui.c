///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     util.c
//
// Abstract:
//
//     Implements the main GUI of KexCfg.
//
// Author:
//
//     vxiiduu (09-Feb-2024)
//
// Environment:
//
//     Win32 mode. This part of the program is not run as SYSTEM.
//
// Revision History:
//
//     vxiiduu              09-Feb-2024  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kexcfg.h"
#include "resource.h"
#include <KexGui.h>
#include <ShlObj.h>
#include <sddl.h>

STATIC HWND MainWindow = NULL;
STATIC HWND ListViewWindow = NULL;
STATIC BOOLEAN UnsavedChanges;
STATIC BOOLEAN SaveInProgress;
STATIC WCHAR SaveCommand[32767];

STATIC BOOL AppendSaveArgument(PCWSTR Argument)
{
    if (KexCfgAppendArgument(SaveCommand, ARRAYSIZE(SaveCommand), Argument)) return TRUE;
    ErrorBoxF(L"The selected applications exceed the command length limit. No settings were changed.");
    return FALSE;
}

STATIC BOOL RunSaveCommand(VOID)
{
    DWORD Error;
    SaveInProgress = TRUE;
    Error = KexCfgRunWriter(MainWindow, SaveCommand);
    SaveInProgress = FALSE;
    if (Error && Error != ERROR_CANCELLED)
        ErrorBoxF(L"The settings could not be applied. %s", Win32ErrorAsString(Error));
    return Error == ERROR_SUCCESS;
}
STATIC BOOLEAN SortOrderIsDescendingForColumn[2] = {0};

STATIC BOOLEAN RequireFullPath(PCWSTR Path)
{
	if (!Path || PathIsRelative(Path)) {
		ErrorBoxF(L"This existing setting has no recorded file location. Add the executable by its full path before managing it here.");
		return FALSE;
	}
	return TRUE;
}

STATIC VOID KexCfgGuiPopulateGlobalConfiguration(
	VOID)
{
	BOOLEAN ExtendedContextMenu;
	BOOLEAN LoggingEnabled;
	WCHAR LogDir[MAX_PATH];

	//
	// Populate the Logging control group
	//

	KxCfgQueryLoggingSettings(&LoggingEnabled, LogDir, ARRAYSIZE(LogDir));
	CheckDlgButton(MainWindow, IDC_ENABLELOGGING, LoggingEnabled);
	SetDlgItemText(MainWindow, IDC_LOGDIR, LogDir);

	//
	// Populate the System Integration control group
	//

	CheckDlgButton(MainWindow, IDC_ENABLEFORMSI, KxCfgQueryVxKexEnabledForMsiexec());
	CheckDlgButton(MainWindow, IDC_CPIWBYPA, KxCfgQueryExplorerCpiwBypass());

	if (KxCfgQueryShellContextMenuEntries(&ExtendedContextMenu)) {
		CheckDlgButton(MainWindow, IDC_ADDTOMENU, TRUE);
		ComboBox_SetCurSel(GetDlgItem(MainWindow, IDC_WHICHCONTEXTMENU), ExtendedContextMenu ? 0 : 1);
	} else {
		CheckDlgButton(MainWindow, IDC_ADDTOMENU, FALSE);
	}
}

STATIC VOID KexCfgGuiApplyGlobalConfiguration(VOID)
{
    HANDLE Token = NULL; PTOKEN_USER User = NULL; PWSTR Sid = NULL;
    DWORD Bytes = 0, Error = 0, Length;
    WCHAR LogDir[MAX_PATH], Resolved[MAX_PATH], SidArgument[256], LogArgument[MAX_PATH + 16];
    if (GetWindowTextLength(GetDlgItem(MainWindow, IDC_LOGDIR)) >= MAX_PATH) {
        ErrorBoxF(L"The log directory is too long."); return;
    }
    GetDlgItemText(MainWindow, IDC_LOGDIR, LogDir, ARRAYSIZE(LogDir));
    // Expand in the original user's environment, before the UAC handoff.
    Length = ExpandEnvironmentStrings(LogDir, Resolved, ARRAYSIZE(Resolved));
    if (!Length || Length > ARRAYSIZE(Resolved) || !*Resolved || PathIsRelative(Resolved)) {
        ErrorBoxF(L"Specify an absolute log directory within the supported path length."); return;
    }
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token)) { Error = GetLastError(); goto Done; }
    GetTokenInformation(Token, TokenUser, NULL, 0, &Bytes);
    User = HeapAlloc(GetProcessHeap(), 0, Bytes);
    if (!User) { Error = ERROR_NOT_ENOUGH_MEMORY; goto Done; }
    if (!GetTokenInformation(Token, TokenUser, User, Bytes, &Bytes) ||
        !ConvertSidToStringSid(User->User.Sid, &Sid)) { Error = GetLastError(); goto Done; }
    if (FAILED(StringCchPrintf(SidArgument, ARRAYSIZE(SidArgument), L"/USER-SID:%s", Sid)) ||
        FAILED(StringCchPrintf(LogArgument, ARRAYSIZE(LogArgument), L"/LOGDIR:%s", Resolved))) {
        Error = ERROR_INSUFFICIENT_BUFFER; goto Done;
    }
    StringCchPrintf(SaveCommand, ARRAYSIZE(SaveCommand),
        L"/GLOBAL /LOGGING:%u /MSI:%u /CONTEXTMENU:%u /EXTENDED:%u",
        !!IsDlgButtonChecked(MainWindow, IDC_ENABLELOGGING),
        !!IsDlgButtonChecked(MainWindow, IDC_ENABLEFORMSI),
        !!IsDlgButtonChecked(MainWindow, IDC_ADDTOMENU),
        ComboBox_GetCurSel(GetDlgItem(MainWindow, IDC_WHICHCONTEXTMENU)) == 0);
    if (AppendSaveArgument(SidArgument) && AppendSaveArgument(LogArgument) && RunSaveCommand())
        UnsavedChanges = FALSE;
Done:
    if (Sid) LocalFree(Sid); if (User) HeapFree(GetProcessHeap(), 0, User); if (Token) CloseHandle(Token);
    if (Error) ErrorBoxF(L"The initiating user could not be identified. %s", Win32ErrorAsString(Error));
}

BOOLEAN CALLBACK ConfigurationCallback(
	IN	PCWSTR							ExeFullPathOrBaseName,
	IN	BOOLEAN							IsLegacyConfiguration,
	IN PVOID ExtraParameter)
{
	WCHAR ExeContainingFolder[MAX_PATH];
	PWSTR ExeBaseName;
	LVITEM ListViewItem;
	SHFILEINFO ShellFileInfo;
	ULONG_PTR Success;


	StringCchCopy(
		ExeContainingFolder,
		ARRAYSIZE(ExeContainingFolder),
		ExeFullPathOrBaseName);

	if (IsLegacyConfiguration) ExeContainingFolder[0] = 0;
	else PathCchRemoveFileSpec(ExeContainingFolder, ARRAYSIZE(ExeContainingFolder));
	ExeBaseName = PathFindFileName(ExeFullPathOrBaseName);

	if (StringEqualI(ExeBaseName, L"msiexec.exe")) {
		// Special case. This is represented by a checkbox in the system integration
		// group, so we don't show it here.
		return TRUE;
	}

	//
	// Get icon of file. SHGetFileInfo works for all types of files including
	// MSI files, and gives us the same icon that would be displayed in Windows
	// Explorer.
	//

	Success = SHGetFileInfo(
		ExeFullPathOrBaseName,
		0,
		&ShellFileInfo,
		sizeof(ShellFileInfo),
		SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SYSICONINDEX);

	RtlZeroMemory(&ListViewItem, sizeof(ListViewItem));

	if (Success) {
		ListViewItem.mask |= LVIF_IMAGE;
		ListViewItem.iImage = ShellFileInfo.iIcon;
		DestroyIcon(ShellFileInfo.hIcon);
	}

	ListViewItem.mask	   |= LVIF_TEXT;
	ListViewItem.iItem		= INT_MAX;
	ListViewItem.pszText	= ExeBaseName;

	//
	// Insert the item and set text for all columns.
	//

	ListViewItem.iItem = ListView_InsertItem(ListViewWindow, &ListViewItem);
	ListView_SetItemText(ListViewWindow, ListViewItem.iItem, 1, ExeContainingFolder);

	return TRUE;
}

STATIC VOID KexCfgGuiPopulateApplicationList(
	VOID)
{
	SetWindowRedraw(ListViewWindow, FALSE);

	ListView_DeleteAllItems(ListViewWindow);
	KxCfgEnumerateConfiguration(ConfigurationCallback, NULL);

	if (ListView_GetItemCount(ListViewWindow) != 0) {
		// enable Clean button
		EnableWindow(GetDlgItem(MainWindow, IDC_CLEANAPPS), TRUE);

		// reflow column widths
		ListView_SetColumnWidth(ListViewWindow, 0, LVSCW_AUTOSIZE);
		ListView_SetColumnWidth(ListViewWindow, 1, LVSCW_AUTOSIZE_USEHEADER);
	} else {
		// disable Clean button
		EnableWindow(GetDlgItem(MainWindow, IDC_CLEANAPPS), FALSE);
	}

	SetWindowRedraw(ListViewWindow, TRUE);
}

STATIC PCWSTR GetProgramFullPathFromListViewIndex(
	IN	ULONG	ItemIndex)
{
	STATIC WCHAR FilePath[MAX_PATH];
	WCHAR FileName[MAX_PATH];

	ListView_GetItemText(ListViewWindow, ItemIndex, 0, FileName, ARRAYSIZE(FileName));
	ListView_GetItemText(ListViewWindow, ItemIndex, 1, FilePath, ARRAYSIZE(FilePath));
	PathCchAppend(FilePath, ARRAYSIZE(FilePath), FileName);

	return FilePath;
}

STATIC VOID RemoveSelectedPrograms(VOID)
{
    int ItemIndex; BOOL Any = FALSE;
    StringCchCopy(SaveCommand, ARRAYSIZE(SaveCommand), L"/DELETE");
    ItemIndex = ListView_GetNextItem(ListViewWindow, -1, LVNI_SELECTED);
    while (ItemIndex != -1) {
        PCWSTR Path = GetProgramFullPathFromListViewIndex(ItemIndex);
        if (!RequireFullPath(Path) || !AppendSaveArgument(Path)) return;
        Any = TRUE; ItemIndex = ListView_GetNextItem(ListViewWindow, ItemIndex, LVNI_SELECTED);
    }
    if (Any && RunSaveCommand()) KexCfgGuiPopulateApplicationList();
}

STATIC VOID AddProgram(
	VOID)
{
	WCHAR FileNames[1024];
	WCHAR FileFullPath[MAX_PATH];
	WCHAR KexDir[MAX_PATH];
	PCWSTR DirectoryName;
	PCWSTR FileName;
	ULONG DirectoryNameCch;
	OPENFILENAME OpenFileInfo;
	WCHAR FilterString[256] = {0};
	INT Index;

	StringCchCat(FilterString, ARRAYSIZE(FilterString), _(L"Programs (*.exe, *.msi)"));
	StringCchCat(FilterString, ARRAYSIZE(FilterString), L"$*.exe;*.msi$");
	for (Index = 0; FilterString[Index] != L'\0'; ++Index) {
		if (FilterString[Index] == L'$') FilterString[Index] = L'\0';
	}

	//
	// Get a list of one or more programs from the user.
	//

	FileNames[0] = '\0';

	RtlZeroMemory(&OpenFileInfo, sizeof(OpenFileInfo));
	OpenFileInfo.lStructSize		= sizeof(OpenFileInfo);
	OpenFileInfo.hwndOwner			= MainWindow;
	OpenFileInfo.lpstrFilter		= FilterString;
	OpenFileInfo.lpstrFile			= FileNames;
	OpenFileInfo.nMaxFile			= ARRAYSIZE(FileNames);
	OpenFileInfo.lpstrTitle			= _(L"Select Program(s)");
	OpenFileInfo.Flags				= OFN_EXPLORER | OFN_ALLOWMULTISELECT |
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	OpenFileInfo.lpstrDefExt		= L"exe";

	if (GetOpenFileName(&OpenFileInfo) == FALSE) {
		ASSERT (CommDlgExtendedError() == 0);
		return;
	}

	ASSERT (FileNames[0] != '\0');

	DirectoryName = FileNames;
	DirectoryNameCch = (ULONG) wcslen(DirectoryName);
	ASSERT (DirectoryNameCch != 0);
	ASSERT (DirectoryNameCch < MAX_PATH);

	KxCfgGetKexDir(KexDir, ARRAYSIZE(KexDir));

	if (StringBeginsWithI(DirectoryName, KexDir)) {
		ErrorBoxF(_(L"You cannot enable VxKex Vista for programs in the VxKex Vista installation directory."));
		return;
	}

	if (OpenFileInfo.nFileOffset >= DirectoryNameCch) {
		// User selected more than one file.
		RtlCopyMemory(FileFullPath, DirectoryName, DirectoryNameCch * sizeof(WCHAR));
		FileFullPath[DirectoryNameCch] = '\\';
		FileFullPath[DirectoryNameCch + 1] = '\0';
		++DirectoryNameCch;

		FileName = FileNames + DirectoryNameCch;
	} else {
		// User selected only one file.
		FileName = FileNames;
		FileNames[DirectoryNameCch + 1] = '\0';
		DirectoryNameCch = 0;
	}

	ASSERT (FileName[0] != '\0');

	if (StringBeginsWithI(DirectoryName, SharedUserData->NtSystemRoot)
		&& !StringEqualI(PathFindFileName(FileName), L"py.exe")
		&& !StringEqualI(PathFindFileName(FileName), L"pyw.exe")) {
			// program(s) are in the Windows directory - do not allow
			ErrorBoxF(_(L"You cannot enable VxKex Vista for programs in the Windows directory."));
			return;
	}

    StringCchCopy(SaveCommand, ARRAYSIZE(SaveCommand), L"/ADD");
    while (*FileName) {
        if (FAILED(StringCchCopy(FileFullPath + DirectoryNameCch,
                ARRAYSIZE(FileFullPath) - DirectoryNameCch, FileName))) {
            ErrorBoxF(L"The selected file path is too long. No settings were changed."); return;
        }
        if (StringBeginsWithI(FileFullPath, SharedUserData->NtSystemRoot) &&
            !StringEqualI(PathFindFileName(FileFullPath), L"py.exe") &&
            !StringEqualI(PathFindFileName(FileFullPath), L"pyw.exe")) {
            ErrorBoxF(L"You cannot enable VxKex Vista for programs in the Windows directory."); return;
        }
        if (!AppendSaveArgument(FileFullPath)) return;
        FileName += wcslen(FileName) + 1;
    }
    if (RunSaveCommand()) KexCfgGuiPopulateApplicationList();
}

STATIC VOID ProgramNotFoundUserPrompt(
	IN	PCWSTR	ProgramFullPath)
{
	INT UserResponse;
	UserResponse = MessageBoxF(
		TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
		NULL,
		_(FRIENDLYAPPNAME_ENG),
		_(L"The selected program cannot be found"), _(
		L"The program \"%s\" was moved or deleted. "
		L"Would you like to remove this entry from the list of VxKex Vista-enabled applications?"),
		ProgramFullPath);

	if (UserResponse == IDYES) {
		RemoveSelectedPrograms();
	}
}

STATIC VOID OpenSelectedItemLocation(
	VOID)
{
	HRESULT Result;
	ULONG ItemIndex;
	PCWSTR ProgramFullPath;

	ASSERT (IsWindow(ListViewWindow));
	ASSERT (ListView_GetSelectedCount(ListViewWindow) == 1);

	ItemIndex = ListView_GetNextItem(ListViewWindow, -1, LVIS_SELECTED);
	ProgramFullPath = GetProgramFullPathFromListViewIndex(ItemIndex);
	if (!RequireFullPath(ProgramFullPath)) return;

	Result = OpenFileLocation(ProgramFullPath, 0);

	if (FAILED(Result)) {
		if (Result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			//
			// Special case for file not found. We will ask the user whether he wants
			// to remove this entry since the EXE doesn't exist anymore.
			//

			ProgramNotFoundUserPrompt(ProgramFullPath);
		} else {
			// display generic error box
			ErrorBoxF(_(L"Couldn't open file location. %s"), Win32ErrorAsString(Result));
		}
	}
}

STATIC VOID OpenSelectedItemProperties(
	VOID)
{
	BOOLEAN Success;
	PCWSTR ProgramFullPath;
	ULONG ItemIndex;

	ASSERT (ListView_GetSelectedCount(ListViewWindow) == 1);

	ItemIndex = ListView_GetNextItem(ListViewWindow, -1, LVIS_SELECTED);
	ProgramFullPath = GetProgramFullPathFromListViewIndex(ItemIndex);
	if (!RequireFullPath(ProgramFullPath)) return;

	Success = ShowPropertiesDialog(ProgramFullPath, SW_SHOW, FALSE);
	if (!Success) {
		ULONG ErrorCode;

		ErrorCode = GetLastError();

		if (ErrorCode == ERROR_FILE_NOT_FOUND) {
			ProgramNotFoundUserPrompt(ProgramFullPath);
		} else {
			ErrorBoxF(_(L"Couldn't open file properties. %s"), Win32ErrorAsString(ErrorCode));
		}
	}
}

STATIC VOID RunSelectedProgram(
	VOID)
{
	PCWSTR ProgramFullPath;
	PCWSTR ErrorMessage;
	ULONG_PTR ErrorCode;
	ULONG ItemIndex;

	ItemIndex = ListView_GetNextItem(ListViewWindow, -1, LVIS_SELECTED);
	ProgramFullPath = GetProgramFullPathFromListViewIndex(ItemIndex);
	if (!RequireFullPath(ProgramFullPath)) return;

	//
	// Check if CPIW bypass has been applied from a previous call to this function.
	// If not, load cpiwbypa so that we have the ability to run programs with a
	// subsystem version higher than 6.1.
	//

	// Modifying Peb->SpareBits0 will crash programs on Windows 8
	//if (!(NtCurrentPeb()->SpareBits0 & 1)) {
		LoadLibrary(L"cpiwbypa.dll");
		//ASSERT (NtCurrentPeb()->SpareBits0 & 1);
	//}

	// why ShellExecute and not CreateProcess? because we can have .msi files too.
	ErrorCode = (ULONG_PTR) ShellExecute(
		MainWindow,
		L"open",
		ProgramFullPath,
		NULL,
		NULL,
		SW_SHOWDEFAULT);

	ErrorMessage = NULL;

	if (ErrorCode <= 32) {
		switch (ErrorCode) {
		case SE_ERR_FNF:
		case SE_ERR_PNF:
			ProgramNotFoundUserPrompt(ProgramFullPath);
			break;
		case SE_ERR_ACCESSDENIED:
			ErrorMessage = _(L"Access was denied or the executable file format is invalid.");
			break;
		case SE_ERR_OOM:
			ErrorMessage = _(L"There was not enough memory to complete the operation.");
			break;
		case SE_ERR_SHARE:
			ErrorMessage = _(L"A sharing violation occurred.");
			break;
		case SE_ERR_ASSOCINCOMPLETE:
			ErrorMessage = L"SE_ERR_ASSOCINCOMPLETE";
			break;
		case SE_ERR_DDETIMEOUT:
			ErrorMessage = L"SE_ERR_DDETIMEOUT";
			break;
		case SE_ERR_DDEFAIL:
			ErrorMessage = L"SE_ERR_DDEFAIL";
			break;
		case SE_ERR_DDEBUSY:
			ErrorMessage = L"SE_ERR_DDEBUSY";
			break;
		case SE_ERR_NOASSOC:
			ErrorMessage = L"SE_ERR_NOASSOC";
			break;
		case SE_ERR_DLLNOTFOUND:
			ErrorMessage = _(L"The specified DLL was not found.");
			break;
		default:
			ErrorMessage = _(L"An unknown error has occurred.");
			break;
		}
	}

	if (ErrorMessage) {
		ErrorBoxF(L"\"%s\": %s", ProgramFullPath, ErrorMessage);
	}
}

STATIC VOID CleanPrograms(VOID)
{
    int ItemIndex; BOOL Any = FALSE;
    StringCchCopy(SaveCommand, ARRAYSIZE(SaveCommand), L"/CLEAN");
    ItemIndex = ListView_GetNextItem(ListViewWindow, -1, LVNI_ALL);
    while (ItemIndex != -1) {
        PCWSTR Path = GetProgramFullPathFromListViewIndex(ItemIndex);
        // Legacy basename entries cannot be safely associated with a file.
        if (!PathIsRelative(Path)) {
            if (!AppendSaveArgument(Path)) return;
            Any = TRUE;
        }
        ItemIndex = ListView_GetNextItem(ListViewWindow, ItemIndex, LVNI_ALL);
    }
    if (Any && RunSaveCommand()) {
        KexCfgGuiPopulateApplicationList();
        InfoBoxF(L"Cleanup completed. Existing and inaccessible applications retain their settings.");
    }
}

STATIC VOID HandleListViewContextMenu(
	IN	PPOINT	ClickPoint)
{
	HWND HeaderWindow;
	RECT HeaderWindowRect;
	LVHITTESTINFO HitTestInfo;
	ULONG ItemIndex;
	USHORT MenuId;
	INT DefaultMenuItem;
	ULONG MenuSelection;
	ULONG ListViewSelectedCount;

	HeaderWindow = ListView_GetHeader(ListViewWindow);
	GetWindowRect(HeaderWindow, &HeaderWindowRect);

	if (PtInRect(&HeaderWindowRect, *ClickPoint)) {
		// The user has right clicked on the list view header.
		return;
	}

	HitTestInfo.pt = *ClickPoint;
	ScreenToClient(ListViewWindow, &HitTestInfo.pt);
	ItemIndex = ListView_HitTest(ListViewWindow, &HitTestInfo);
	ListViewSelectedCount = ListView_GetSelectedCount(ListViewWindow);
	DefaultMenuItem = -1;

	if (ItemIndex == -1) {
		// The user has right clicked on a blank space in the list view.
		MenuId = IDM_LISTVIEWBLANKSPACEMENU;
	} else {
		// The user has right clicked on one or more items.
		if (ListViewSelectedCount == 1) {
			MenuId = IDM_LISTVIEWITEMMENU;
			DefaultMenuItem = M_OPENFILELOCATION;
		} else if (ListViewSelectedCount > 1) {
			// more than 1 item
			MenuId = IDM_LISTVIEWMULTIITEMMENU;
		} else {
			// can happen if Ctrl key is held down.
			// in this situation, programs like Windows Explorer will display
			// the same menu as for right clicking on blank space.
			MenuId = IDM_LISTVIEWBLANKSPACEMENU;
		}
	}

	MenuSelection = ContextMenuEx(ListViewWindow, MenuId, ClickPoint, DefaultMenuItem);
	if (!MenuSelection) {
		return;
	}

	if (MenuSelection == M_OPENFILELOCATION) {
		ASSERT (ListViewSelectedCount == 1);
		OpenSelectedItemLocation();
	} else if (MenuSelection == M_RUNPROGRAM) {
		ASSERT (ListViewSelectedCount == 1);
		RunSelectedProgram();
	} else if (MenuSelection == M_PROPERTIES) {
		ASSERT (ListViewSelectedCount == 1);
		OpenSelectedItemProperties();
	} else if (MenuSelection == M_REMOVE) {
		RemoveSelectedPrograms();
	} else if (MenuSelection == M_ADDPROGRAM) {
		AddProgram();
	} else if (MenuSelection == M_CLEANPROGRAMS) {
		CleanPrograms();
	}
}

STATIC INT CALLBACK KexCfgGuiSortListViewItems(
	IN	LPARAM	LParam1,
	IN	LPARAM	LParam2,
	IN	LPARAM	ColumnIndex)
{
	WCHAR Text1[MAX_PATH];
	WCHAR Text2[MAX_PATH];
	ULONG Length1;
	ULONG Length2;
	INT ComparisonResult;

	ListView_GetItemTextEx(ListViewWindow, LParam1, (INT) ColumnIndex, Text1, ARRAYSIZE(Text1), &Length1);
	ListView_GetItemTextEx(ListViewWindow, LParam2, (INT) ColumnIndex, Text2, ARRAYSIZE(Text2), &Length2);

	ComparisonResult = CompareString(
		LOCALE_USER_DEFAULT,
		0,
		Text1,
		Length1,
		Text2,
		Length2);

	ASSERT (ComparisonResult != 0);
	ComparisonResult -= 2;

	ASSERT (ColumnIndex < ARRAYSIZE(SortOrderIsDescendingForColumn));

	if (SortOrderIsDescendingForColumn[ColumnIndex]) {
		ComparisonResult = -ComparisonResult;
	}

	return ComparisonResult;
}

STATIC INT_PTR CALLBACK DialogProc(
	IN	HWND	Window,
	IN	UINT	Message,
	IN	WPARAM	WParam,
	IN	LPARAM	LParam)
{
	if (SaveInProgress && (Message == WM_COMMAND || Message == WM_CLOSE || Message == WM_CONTEXTMENU)) return TRUE;
	if (Message == WM_INITDIALOG) {
		HMODULE Uxtheme;
		HRESULT (WINAPI *pEnableThemeDialogTexture) (HWND, DWORD);
		BOOLEAN Success;
		HWND ComboBoxWindow;
		HWND LogDirWindow;
		LVCOLUMN Column;
		HIMAGELIST SystemSmallImageList = NULL;
		HIMAGELIST SystemLargeImageList = NULL;

		MainWindow = Window;
		KexgApplicationMainWindow = Window;
		SetWindowIcon(Window, IDI_APPICON);

		//
		// Translate static window text with MLS.
		//

		MlsgTranslateWindow(Window);

		Uxtheme = LoadLibrary(L"uxtheme.dll");
		pEnableThemeDialogTexture = (HRESULT (WINAPI *) (HWND, DWORD)) GetProcAddress(Uxtheme, "EnableThemeDialogTexture");
		if (pEnableThemeDialogTexture) pEnableThemeDialogTexture(Window, ETDT_ENABLE | ETDT_USETABTEXTURE);
		FreeLibrary(Uxtheme);

		//
		// Limit the max length of the log directory path to 200 characters.
		// Make the path edit box autocomplete paths.
		//

		LogDirWindow = GetDlgItem(Window, IDC_LOGDIR);
		Edit_LimitText(LogDirWindow, 200);
		SHAutoComplete(LogDirWindow, SHACF_FILESYS_DIRS | SHACF_USETAB);

		//
		// Initialize the context menu selection combo box.
		//

		ComboBoxWindow = GetDlgItem(Window, IDC_WHICHCONTEXTMENU);
		ComboBox_AddString(ComboBoxWindow, _(L"extended context menu"));
		ComboBox_AddString(ComboBoxWindow, _(L"normal context menu"));
		ComboBox_SetCurSel(ComboBoxWindow, 0);

		//
		// Initialize the application list view.
		//

		ListViewWindow = GetDlgItem(Window, IDC_LISTVIEW);
		SetWindowTheme(ListViewWindow, L"Explorer", NULL);

		Success = Shell_GetImageLists(&SystemLargeImageList, &SystemSmallImageList);
		ASSERT (Success);

		if (Success) {
			ListView_SetImageList(ListViewWindow, SystemSmallImageList, LVSIL_SMALL);
			ListView_SetImageList(ListViewWindow, SystemLargeImageList, LVSIL_NORMAL);
		}

		ListView_SetExtendedListViewStyle(
			ListViewWindow,
			LVS_EX_AUTOCHECKSELECT |
			LVS_EX_CHECKBOXES |
			LVS_EX_FULLROWSELECT |
			LVS_EX_LABELTIP |
			LVS_EX_DOUBLEBUFFER);

		RtlZeroMemory(&Column, sizeof(Column));
		Column.mask = LVCF_TEXT | LVCF_WIDTH;

		Column.pszText = (LPWSTR)_(L"Application");
		Column.cx = DpiScaleX(100);
		ListView_InsertColumn(ListViewWindow, 0, &Column);

		Column.pszText = (LPWSTR)_(L"Containing folder");
		ListView_InsertColumn(ListViewWindow, 1, &Column);
		ListView_SetColumnWidth(ListViewWindow, 1, LVSCW_AUTOSIZE_USEHEADER);

		//
		// Add tool tips.
		//

		ToolTip(Window, IDC_ENABLELOGGING, (PWSTR)_(
			L"If you enable logging, VxKex Vista will create log files in the specified "
			L"folder every time you run an application which has VxKex Vista enabled."));
		ToolTip(Window, IDC_ENABLEFORMSI, (PWSTR)_(
			L"This option allows VxKex Vista to work with MSI installers.\r\n"
			L"If you encounter unexpected problems with MSI installers, try disabling this option."));
		ToolTip(Window, IDC_CPIWBYPA, (PWSTR)_(
			L"This option causes a DLL to be loaded into Windows Explorer at startup in order "
			L"to remove the version check for certain programs.\r\n"
			L"If you encounter unexpected problems with Windows Explorer, try disabling this "
			L"option."));
		ToolTip(Window, IDC_ADDTOMENU, (PWSTR)_(
			L"Add \"Run with VxKex Vista\" options to the context menu for .exe and .msi files."));
		ToolTip(Window, IDC_WHICHCONTEXTMENU, (PWSTR)_(
			L"Shift + Right Click on a .exe or .msi file opens the extended context menu.\r\n"
			L"Right clicking without holding the Shift key opens the normal context menu."));
		ToolTip(Window, IDC_CLEANAPPS, (PWSTR)_(
			L"Applications which no longer exist may remain enabled in VxKex Vista. Click this button "
			L"in order to remove deleted EXEs or MSIs from the list."));

		//
		// Populate the top section (global configuration) and the apps list.
		//

		KexCfgGuiPopulateGlobalConfiguration();
		EnableWindow(GetDlgItem(Window, IDC_CPIWBYPA), FALSE);
		KexCfgGuiPopulateApplicationList();

		//
		// Update the state of certain interdependent controls (i.e. controls
		// that are supposed to be enabled/disabled based on the state of a
		// checkbox)
		//

		DialogProc(Window, WM_COMMAND, IDC_ENABLELOGGING, 0);
		DialogProc(Window, WM_COMMAND, IDC_ADDTOMENU, 0);

		UnsavedChanges = FALSE;
		EnableWindow(GetDlgItem(Window, IDC_APPLY), FALSE);
	} else if (Message == WM_COMMAND) {
		ULONG ControlId;
		ULONG NotificationCode;

		ControlId = LOWORD(WParam);
		NotificationCode = HIWORD(WParam);

		if (ControlId == IDC_ENABLELOGGING ||
			ControlId == IDC_ENABLEFORMSI ||
			ControlId == IDC_CPIWBYPA ||
			ControlId == IDC_ADDTOMENU ||
			(ControlId == IDC_WHICHCONTEXTMENU && NotificationCode == CBN_SELCHANGE) ||
			(ControlId == IDC_LOGDIR && NotificationCode == EN_CHANGE)) {

				if (UnsavedChanges == FALSE) {
					UnsavedChanges = TRUE;
					EnableWindow(GetDlgItem(Window, IDC_APPLY), TRUE);
				}
		}

		if (ControlId == IDC_CANCEL) {
			if (UnsavedChanges) {
				INT UserResponse;

				UserResponse = MessageBoxF(
					TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
					0,
					_(FRIENDLYAPPNAME_ENG),
					_(L"Are you sure you want to exit?"),
					_(L"Changes that are not applied will be discarded."));

				if (UserResponse == IDYES) {
					EndDialog(Window, 0);
				}
			} else {
				EndDialog(Window, 0);
			}
		} else if (ControlId == IDC_OK) {
			if (UnsavedChanges) {
				KexCfgGuiApplyGlobalConfiguration();
			}

			unless (UnsavedChanges) {
				EndDialog(Window, 0);
			}
		} else if (ControlId == IDC_APPLY) {
			if (UnsavedChanges) {
				KexCfgGuiApplyGlobalConfiguration();
			}

			unless (UnsavedChanges) {
				EnableWindow(GetDlgItem(Window, ControlId), FALSE);
			}
		} else if (ControlId == IDC_BROWSELOGDIR) {
			BOOLEAN Success;
			WCHAR NewLogDir[MAX_PATH];

			GetDlgItemText(Window, IDC_LOGDIR, NewLogDir, ARRAYSIZE(NewLogDir));
			Success = PickFolder(Window, NULL, 0, NewLogDir, ARRAYSIZE(NewLogDir));

			if (Success) {
				SetDlgItemText(Window, IDC_LOGDIR, NewLogDir);
			}
		} else if (ControlId == IDC_ENABLELOGGING) {
			BOOLEAN LoggingEnabled;

			LoggingEnabled = IsDlgButtonChecked(Window, ControlId);

			EnableWindow(GetDlgItem(Window, IDC_LOGDIR), LoggingEnabled);
			EnableWindow(GetDlgItem(Window, IDC_BROWSELOGDIR), LoggingEnabled);
		} else if (ControlId == IDC_ADDTOMENU) {
			EnableWindow(
				GetDlgItem(Window, IDC_WHICHCONTEXTMENU),
				IsDlgButtonChecked(Window, ControlId));
		} else if (ControlId == IDC_NEWAPP) {
			AddProgram();
		} else if (ControlId == IDC_REMOVEAPPS) {
			RemoveSelectedPrograms();
		} else if (ControlId == IDC_PROPERTIES) {
			OpenSelectedItemProperties();
		} else if (ControlId == IDC_CLEANAPPS) {
			CleanPrograms();
		} else {
			return FALSE;
		}
	} else if (Message == WM_NOTIFY) {
		LPNMHDR Notification;

		Notification = (LPNMHDR) LParam;

		if (Notification->idFrom == IDC_LISTVIEW) {
			if (Notification->code == LVN_ITEMCHANGED) {
				ULONG ListViewSelectedCount;

				ListViewSelectedCount = ListView_GetSelectedCount(ListViewWindow);

				if (ListViewSelectedCount == 0) {
					// no items selected
					EnableWindow(GetDlgItem(Window, IDC_REMOVEAPPS), FALSE);
					EnableWindow(GetDlgItem(Window, IDC_PROPERTIES), FALSE);
				} else {
					// one or more items selected
					EnableWindow(GetDlgItem(Window, IDC_REMOVEAPPS), TRUE);

					if (ListViewSelectedCount == 1) {
						EnableWindow(GetDlgItem(Window, IDC_PROPERTIES), TRUE);
					} else {
						// no properties button for >1 item selected
						EnableWindow(GetDlgItem(Window, IDC_PROPERTIES), FALSE);
					}
				}
			} else if (Notification->code == LVN_DELETEITEM) {
				// after items are deleted, no items will be selected anymore
				EnableWindow(GetDlgItem(Window, IDC_REMOVEAPPS), FALSE);
				EnableWindow(GetDlgItem(Window, IDC_PROPERTIES), FALSE);
			} else if (Notification->code == NM_DBLCLK) {
				//
				// User double clicked on a list-view item.
				// If Alt is held down, we will open the properties of the file,
				// just like in Windows Explorer.
				// Otherwise, we will open the location of that file.
				//

				if (GetKeyState(VK_MENU) & 0x8000) {
					OpenSelectedItemProperties();
				} else {
					OpenSelectedItemLocation();
				}
			} else if (Notification->code == LVN_COLUMNCLICK) {
				ULONG ColumnIndex;

				ColumnIndex = ((LPNMLISTVIEW) LParam)->iSubItem;

				// toggle sort order
				ASSERT (ColumnIndex < ARRAYSIZE(SortOrderIsDescendingForColumn));
				SortOrderIsDescendingForColumn[ColumnIndex] ^= 1;

				// User has clicked on a column header.
				// We are now going to sort that column.
				ListView_SortItemsEx(
					ListViewWindow,
					KexCfgGuiSortListViewItems,
					ColumnIndex);
			}
		} else {
			return FALSE;
		}
	} else if (Message == WM_CONTEXTMENU) {
		HWND ContextMenuWindow;

		ContextMenuWindow = (HWND) WParam;

		if (ContextMenuWindow == ListViewWindow) {
			POINT ClickPoint;
			ClickPoint.x = GET_X_LPARAM(LParam);
			ClickPoint.y = GET_Y_LPARAM(LParam);
			HandleListViewContextMenu(&ClickPoint);
		}
	} else if (Message == WM_CLOSE) {
		DialogProc(Window, WM_COMMAND, IDC_CANCEL, 0);
	} else {
		return FALSE;
	}

	return TRUE;
}

VOID KexCfgOpenGUI(
	VOID)
{
	INITCOMMONCONTROLSEX Controls = {sizeof(Controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
	HRESULT Result;
	INT_PTR DialogResult;
	if (!InitCommonControlsEx(&Controls)) return;
	Result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(Result)) return;
	KexgApplicationFriendlyName = _(FRIENDLYAPPNAME_ENG);
	DialogResult = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc);
	if (DialogResult == -1) ErrorBoxF(L"Could not open configuration window. %s", GetLastErrorAsString());
	CoUninitialize();
}
