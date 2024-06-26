#include <Windows.h>
#include <stdio.h>
#include "resource.h"

BOOL bExportbyOrdinal;
BOOL bOpenDEF;
BOOL bCapitalize;
BOOL bHideMsg;

HWND hBtnExit;
HWND hListbox;
WCHAR sDraggedFile[MAX_PATH];
WCHAR sDefFile[MAX_PATH];
WCHAR sLibFile[MAX_PATH];
WCHAR sMsg[MAX_PATH];
WCHAR sErrorMsgOrCMD[MAX_PATH];
WCHAR lib文件所在目录[MAX_PATH];
WCHAR* 导出平台;

HANDLE hFile;
DWORD nFileSize;
HANDLE hFileMapping;
BYTE* pFile;

IMAGE_DOS_HEADER* pDosHd;
DWORD* pSignature;
IMAGE_FILE_HEADER* pFileHeader;
IMAGE_OPTIONAL_HEADER32* pOptionalHeader32;
IMAGE_OPTIONAL_HEADER64* pOptionalHeader64;
IMAGE_SECTION_HEADER* pScnHd;

IMAGE_EXPORT_DIRECTORY* pExpD;
DWORD* pNameRVA;
WORD* pOrdinal;

PSTR* pName;
WORD* pwName;
WCHAR** ppwName;
int* pIndex;

BYTE* pDllName;
CHAR pDllNameDecorated[MAX_PATH];
FILE* fDEF;

STARTUPINFO si;
PROCESS_INFORMATION pi;

#define WM_USER_ARG		WM_USER+1

VOID AddItemtoListbox(TCHAR *sMsg)
{
	SendMessage(hListbox, LB_ADDSTRING, 0, (LPARAM)sMsg);
	// SendMessage(hListbox, LB_SETCURSEL, SendMessage(hListbox, LB_GETCOUNT, 0, 0) - 1, 0);
}
VOID ClearListboxItems()
{
	UINT n = SendMessage(hListbox, LB_GETCOUNT, 0, 0);
	while (n > 0)
	{
		SendMessage(hListbox, LB_DELETESTRING, n - 1, 0);
		--n;
	} 
}
VOID PrintErrorMessage(WCHAR* sError)
{
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), sErrorMsgOrCMD, MAX_PATH, NULL);
	swprintf_s(sMsg, MAX_PATH, sError, sErrorMsgOrCMD);
	AddItemtoListbox(sMsg);
}
#ifndef INVALID_OFFSET
#define INVALID_OFFSET 0xFFFFFFFF
#endif
DWORD RVAToFileOffset(DWORD RVA, IMAGE_SECTION_HEADER* pScnHd, DWORD NumberOfSections)
{
	if (RVA < pScnHd[0].VirtualAddress)
		return RVA;
	else
	{
		DWORD i;
		for (i = 0; i < NumberOfSections; ++i)
			if (RVA >= pScnHd[i].VirtualAddress && RVA < pScnHd[i].VirtualAddress + pScnHd[i].Misc.VirtualSize)
				return RVA - (pScnHd[i].VirtualAddress - pScnHd[i].PointerToRawData);
		return INVALID_OFFSET;
	}
}
VOID GetDefPathFromDllPath(PWSTR sDefPath, PWSTR sFilePath)
{
	PWSTR sC;
	wcscpy_s(sDefPath, MAX_PATH, sFilePath);
	sC = wcsrchr(sDefPath, '.');
	if (sC)
	{
		++sC;
		*sC++ = 'd';
		*sC++ = 'e';
		*sC = 'f';
	}
}
VOID GetLibPathFromDllPath(PWSTR sLibPath, PWSTR sFilePath)
{
	PWSTR sC;
	wcscpy_s(sLibPath, MAX_PATH, sFilePath);
	sC = wcsrchr(sLibPath, '.');
	if (sC)
	{
		++sC;
		*sC++ = 'l';
		*sC++ = 'i';
		*sC = 'b';
	}
}
VOID DecorateStringA(PSTR pS) // 将首字母大写，其余字母小写
{
	if (*pS >= 'a' && *pS <= 'z')
		*pS &= 0xDF;
	++pS;
	for (; *pS; ++pS)
		if (*pS >= 'A' && *pS <= 'Z')
			*pS |= 0x20;
}
VOID DecorateStringW(PWSTR pS) // 将首字母大写，其余字母小写
{
	PWSTR p = wcsrchr(pS, '\\');
	if (p)
	{
		++p;
		pS = p;
	}

	if (*pS >= 'a' && *pS <= 'z')
		*pS &= 0xDF;
	++pS;
	for (; *pS; ++pS)
		if (*pS >= 'A' && *pS <= 'Z')
			*pS |= 0x20;
}
#ifndef IMAGE_SIZEOF_NT_OPTIONAL32_HEADER
#define IMAGE_SIZEOF_NT_OPTIONAL32_HEADER 224
#endif
VOID DLL2DEF()
{
	if (_wcsicmp(wcsrchr(sDraggedFile, '.'), L".dll") == 0)
	{
		hFile = CreateFile(sDraggedFile, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			nFileSize = GetFileSize(hFile, NULL);
			if (nFileSize != INVALID_FILE_SIZE)
			{
				if (nFileSize)
				{
					hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
					if (hFileMapping)
					{
						pFile = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
						if (pFile)
						{
							pDosHd = (IMAGE_DOS_HEADER*)pFile;

							if (pDosHd->e_magic == IMAGE_DOS_SIGNATURE)
							{
								/*
								swprintf_s(sMsg, MAX_PATH, L"IMAGE_DOS_HEADER Located: [%02X %02X] '%c%c'", (BYTE)pDosHd->e_magic, (BYTE)(pDosHd->e_magic >> 8), (BYTE)pDosHd->e_magic, (BYTE)(pDosHd->e_magic >> 8));
								AddItemtoListbox(sMsg);
								*/
								if (pDosHd->e_lfanew >= 4)
								{
									if (pDosHd->e_lfanew < (LONG)nFileSize)
									{
										// Signature
										pSignature = (DWORD*)(pFile + pDosHd->e_lfanew);

										if (*pSignature == IMAGE_NT_SIGNATURE)
										{
											DWORD i, k;

											pFileHeader = (IMAGE_FILE_HEADER*)(pFile + pDosHd->e_lfanew + 4);
											pOptionalHeader32 = (IMAGE_OPTIONAL_HEADER32*)(pFile + pDosHd->e_lfanew + 4 + IMAGE_SIZEOF_FILE_HEADER);
											pOptionalHeader64 = (IMAGE_OPTIONAL_HEADER64*)(pFile + pDosHd->e_lfanew + 4 + IMAGE_SIZEOF_FILE_HEADER);

											if (pFileHeader->Machine == IMAGE_FILE_MACHINE_I386 && pOptionalHeader32->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) // I386 && PE32
											{
												pOptionalHeader64 = NULL;
												pScnHd = (IMAGE_SECTION_HEADER*)(pFile + pDosHd->e_lfanew + 4 + IMAGE_SIZEOF_FILE_HEADER + sizeof(IMAGE_OPTIONAL_HEADER32)); // +224

												if (pOptionalHeader32->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
												{
													pExpD = (IMAGE_EXPORT_DIRECTORY*)(pFile + RVAToFileOffset(pOptionalHeader32->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, pScnHd, pFileHeader->NumberOfSections));

													swprintf_s(sMsg, MAX_PATH, L"PE32 File: %s", sDraggedFile);
													AddItemtoListbox(sMsg);
												}
												else
												{
													AddItemtoListbox(L"Export Table Not Found!");
													goto END;
												}
											}
											else if (pFileHeader->Machine == IMAGE_FILE_MACHINE_AMD64 && pOptionalHeader64->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) // AMD64 && PE32+
											{
												pOptionalHeader32 = NULL;
												pScnHd = (IMAGE_SECTION_HEADER*)(pFile + pDosHd->e_lfanew + 4 + IMAGE_SIZEOF_FILE_HEADER + sizeof(IMAGE_OPTIONAL_HEADER64)); // +240

												if (pOptionalHeader64->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
												{
													pExpD = (IMAGE_EXPORT_DIRECTORY*)(pFile + RVAToFileOffset(pOptionalHeader64->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, pScnHd, pFileHeader->NumberOfSections));

													swprintf_s(sMsg, MAX_PATH, L"PE32+ File: %s", sDraggedFile);
													AddItemtoListbox(sMsg);
												}
												else
												{
													AddItemtoListbox(L"Export Table Not Found!");
													goto END;
												}

											}
											else
											{
												AddItemtoListbox(L"Neither I386 PE32 File nor AMD64 PE32+ File!");
												goto END;
											}

											#pragma region do_my_work
											AddItemtoListbox(L"Analysis started");
											if (bHideMsg == FALSE)
											{
												AddItemtoListbox(L"");
												AddItemtoListbox(L"[Export Directory Table]");
												swprintf_s(sMsg, MAX_PATH, L"Export Flags / Characteristics: %08X", pExpD->Characteristics);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Time/Date Stamp: %08X", pExpD->TimeDateStamp);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Major Version: %04X", pExpD->MajorVersion);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Minor Version: %04X", pExpD->MinorVersion);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Name RVA: %08X", pExpD->Name);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Ordinal Base: %08X", pExpD->Base);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Address Table Entries / NumberOfFunctions: %08X", pExpD->NumberOfFunctions);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Number of Name Pointers / NumberOfNames: %08X", pExpD->NumberOfNames);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Export Address Table RVA / AddressOfFunctions: %08X", pExpD->AddressOfFunctions);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Name Pointer RVA / AddressOfNames: %08X", pExpD->AddressOfNames);
												AddItemtoListbox(sMsg);
												swprintf_s(sMsg, MAX_PATH, L"Ordinal Table RVA / AddressOfNameOrdinals: %08X", pExpD->AddressOfNameOrdinals);
												AddItemtoListbox(sMsg);
											}

											pNameRVA = (DWORD*)(pFile + RVAToFileOffset(pExpD->AddressOfNames, pScnHd, pFileHeader->NumberOfSections));
											pOrdinal = (WORD *)(pFile + RVAToFileOffset(pExpD->AddressOfNameOrdinals, pScnHd, pFileHeader->NumberOfSections));

											pName = malloc(sizeof(PSTR*) * pExpD->NumberOfNames);
											pwName = malloc(sizeof(WORD) * MAX_PATH * pExpD->NumberOfNames);
											ppwName = malloc(sizeof(WCHAR*) * pExpD->NumberOfNames);
											for (i = 0; i < pExpD->NumberOfNames; ++i)
												ppwName[i] = pwName + (DWORD)(MAX_PATH * i);

											for (i = 0; i < pExpD->NumberOfNames; ++i)
											{
												pName[i] = pFile + RVAToFileOffset(pNameRVA[i], pScnHd, pFileHeader->NumberOfSections);
												MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pName[i], -1, ppwName[i], MAX_PATH);
												// swprintf_s(sMsg, MAX_PATH, L"%d %s", i, ppwName[i]);
												// AddItemtoListbox(sMsg);
											}
											/*
											AddItemtoListbox(L"");
											AddItemtoListbox(L"===================Exported Functions===================");
											for (i = 0; i < pExpD->NumberOfNames; ++i)
											{
												swprintf_s(sMsg, MAX_PATH, L"[%d] %08X %s @ %d", i, pNameRVA[i], ppwName[i], pOrdinal[i] + pExpD->Base);
												AddItemtoListbox(sMsg);
											}
											*/
											//
											pIndex = malloc(sizeof(int*) * pExpD->NumberOfFunctions);

											for (i = 0; i < pExpD->NumberOfFunctions; ++i)
											{
												pIndex[i] = -1;

												for (k = 0; k < pExpD->NumberOfNames; ++k)
												{
													if (i == pOrdinal[k])
													{
														pIndex[i] = k;
														break;
													}
												}
											}


											// Use sDraggedFile as sFileNameDef
											GetDefPathFromDllPath(sDefFile, sDraggedFile);
											GetLibPathFromDllPath(sLibFile, sDraggedFile);
											if (bCapitalize)
											{
												DecorateStringW(sDefFile);
												DecorateStringW(sLibFile);
											}
											pDllName = pFile + RVAToFileOffset(pExpD->Name, pScnHd, pFileHeader->NumberOfSections);
											_wfopen_s(&fDEF, sDefFile, L"w");

											strcpy_s(pDllNameDecorated, MAX_PATH, pDllName);
											if (bCapitalize)
												DecorateStringA(pDllNameDecorated);

											if (fDEF)
											{
												fprintf(fDEF, "LIBRARY %s\nEXPORTS\n", pDllNameDecorated);
												for (i = 0; i < pExpD->NumberOfFunctions; ++i)
												{
													if (pIndex[i] != -1)
													{
														fprintf(fDEF, "\n\t%s", pName[pIndex[i]]);
														if (bExportbyOrdinal)
															fprintf(fDEF, "\t@%d", i + pExpD->Base);
													}
												}
												fclose(fDEF);
											}

											if (bOpenDEF)
											{
												si.cb = sizeof(STARTUPINFO);
												si.wShowWindow = SW_SHOW;
												si.dwFlags = STARTF_USESHOWWINDOW;
												swprintf_s(sMsg, MAX_PATH, L"Notepad %s", sDefFile);
												CreateProcess(NULL, sMsg, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
											}

											swprintf_s(sMsg, MAX_PATH, L"Generated DEF File: %s", sDefFile);
											AddItemtoListbox(sMsg);

											si.cb = sizeof(STARTUPINFO);
											si.wShowWindow = SW_HIDE;
											si.dwFlags = STARTF_USESHOWWINDOW;
											GetModuleFileName(NULL, lib文件所在目录, MAX_PATH);

											// 假使路径一定含 '\'
											*(wcsrchr(lib文件所在目录, '\\') + 1) = '\0';

											if (!pOptionalHeader64)
												导出平台 = L"X86";
											if (!pOptionalHeader32)
												导出平台 = L"X64";
											swprintf_s(sErrorMsgOrCMD, MAX_PATH, L"CMD /C %slib.exe /DEF:%s /MACHINE:%s /NOLOGO /OUT:%s", lib文件所在目录, sDefFile, 导出平台, sLibFile);
											CreateProcess(NULL, sErrorMsgOrCMD, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

											//swprintf_s(sMsg, MAX_PATH, L"[%s]", sErrorMsgOrCMD);
											//AddItemtoListbox(sMsg);

											if (!pOptionalHeader64)
												swprintf_s(sMsg, MAX_PATH, L"Generated IMPORT LIB for X86 DLL: %s", sLibFile);
											if (!pOptionalHeader32)
												swprintf_s(sMsg, MAX_PATH, L"Generated IMPORT LIB for X64 DLL: %s", sLibFile);
											AddItemtoListbox(sMsg);

											free(pIndex);
											free(ppwName);
											free(pwName);
											free(pName);

											END:
											AddItemtoListbox(L"Done");
											#pragma endregion
										}
										else
										{
											AddItemtoListbox(L"Signature != IMAGE_NT_SIGNATURE (PE\\0\\0)");
										}
									}
									else
									{
										AddItemtoListbox(L"pDosHd->e_lfanew >= nFileSize");
									}
								}
								else
								{
									AddItemtoListbox(L"pDosHd->e_lfanew < 4");
								}
							}
							else
							{
								AddItemtoListbox(L"pDosHd->e_magic != 0x5A4D");
							}
							UnmapViewOfFile(pFile);
						}
						else
						{
							PrintErrorMessage(L"MapViewOfFile Failed: %s");
						}
						CloseHandle(hFileMapping);
					}
					else
					{
						PrintErrorMessage(L"CreateFileMapping Failed: %s");
						/*
						FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), sErrorMsgOrCMD, MAX_PATH, NULL);
						swprintf_s(sMsg, MAX_PATH, L"CreateFileMapping Failed: %s", sErrorMsgOrCMD);
						AddItemtoListbox(sMsg);
						*/
					}
				}
				else
				{
					AddItemtoListbox(L"Empty File Detected");
				}
			}
			else
			{
				PrintErrorMessage(L"GetFileSize Failed: %s");
			}
			CloseHandle(hFile);
		}
		else
		{
			PrintErrorMessage(L"CreateFile Failed: %s");
		}
	}
}
LRESULT CALLBACK DlgFunc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		DragAcceptFiles(hDlg, TRUE);
		hListbox = GetDlgItem(hDlg, IDC_LIST_LOGS);
		bExportbyOrdinal = FALSE;
		bCapitalize = TRUE;
		bHideMsg = TRUE;

		SendDlgItemMessage(hDlg, IDC_CHECK_CAPITALIZE, BM_SETCHECK, BST_CHECKED, 0);
		SendDlgItemMessage(hDlg, IDC_HIDE_EDT_MSG, BM_SETCHECK, BST_CHECKED, 0);
		if (lParam)
		{
			bOpenDEF = FALSE;
			SendDlgItemMessage(hDlg, IDC_CHECK_OPENDEF, BM_SETCHECK, BST_UNCHECKED, 0);
			wcscpy_s(sDraggedFile, MAX_PATH, (PWSTR)lParam);
			SendMessage(hDlg, WM_USER_ARG, 0, 0);
		}
		else
		{
			bOpenDEF = TRUE;
			SendDlgItemMessage(hDlg, IDC_CHECK_OPENDEF, BM_SETCHECK, BST_CHECKED, 0);
		}
		break;
		
	case WM_DROPFILES:
		EnableWindow(hBtnExit, FALSE);
		DragQueryFile((HDROP)wParam, 0, sDraggedFile, MAX_PATH);
		DragFinish((HDROP)wParam);
		ClearListboxItems();
		DLL2DEF();
		EnableWindow(hBtnExit, TRUE);
		break;

	case WM_CLOSE:
		EndDialog(hDlg, IDCANCEL);
		break;
		
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CHECK_BYORDINAL:
			if (SendDlgItemMessage(hDlg, IDC_CHECK_BYORDINAL, BM_GETCHECK, 0, 0) == BST_CHECKED)
				bExportbyOrdinal = TRUE;
			else
				bExportbyOrdinal = FALSE;
			break;

		case IDC_CHECK_OPENDEF:
			if (SendDlgItemMessage(hDlg, IDC_CHECK_OPENDEF, BM_GETCHECK, 0, 0) == BST_CHECKED)
				bOpenDEF = TRUE;
			else
				bOpenDEF = FALSE;
			break;

		case IDC_CHECK_CAPITALIZE:
			if (SendDlgItemMessage(hDlg, IDC_CHECK_CAPITALIZE, BM_GETCHECK, 0, 0) == BST_CHECKED)
				bCapitalize = TRUE;
			else
				bCapitalize = FALSE;
			break;

		case IDC_HIDE_EDT_MSG:
			if (SendDlgItemMessage(hDlg, IDC_HIDE_EDT_MSG, BM_GETCHECK, 0, 0) == BST_CHECKED)
				bHideMsg = TRUE;
			else
				bHideMsg = FALSE;
			break;

		default:
			break;
		}
		break;

	case WM_USER_ARG:
		EnableWindow(hBtnExit, FALSE);
		ClearListboxItems();
		DLL2DEF();
		EnableWindow(hBtnExit, TRUE);
		break;

	default:
		break;
	}
	return 0;
}
VOID EntryPoint()
{
	PWSTR* sArgv;
	INT nArgs;
	LPARAM sDLLfile;

	sArgv = CommandLineToArgvW(GetCommandLine(), &nArgs);
	if (sArgv)
	{
		if (nArgs > 1)
			sDLLfile = (LPARAM)sArgv[1];
		else
			sDLLfile = 0;
		DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG), NULL, DlgFunc, sDLLfile);
		LocalFree(sArgv);
	}
	ExitProcess(0);
}
