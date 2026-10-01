///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     certs.c
//
// Abstract:
//
//     Contains utility functions for dealing with certificates.
//
// Author:
//
//     vxiiduu (14-May-2026)
//
// Environment:
//
//     Win32
//
// Revision History:
//
//     vxiiduu               14-May-2026  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kxschanlp.h"

//
// Load the root certificate bundle into the given KxSchanl credential.
//
BOOLEAN SppLoadRootCertificates(
	IN OUT	PKXSCHANL_CREDENTIAL	Credential)
{
	WCHAR BundlePath[MAX_PATH];
	HRESULT Result;
	DWORD Error;

	Result = StringCchCopy(BundlePath, ARRAYSIZE(BundlePath), KexData->KexDir.Buffer);
	if (FAILED(Result)) {
		return FALSE;
	}
	Result = PathCchAppend(BundlePath, ARRAYSIZE(BundlePath), L"Certificates\\ROOT.sst");
	if (FAILED(Result)) {
		return FALSE;
	}

	Credential->RootCertStore = CertOpenStore(
		CERT_STORE_PROV_FILENAME,
		0,
		0,
		CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG | CERT_STORE_SHARE_CONTEXT_FLAG,
		BundlePath);

	//
	// The bundle is optional. Only its absence permits system-root fallback.
	// A damaged or inaccessible bundle must not silently change the trust policy.
	//
	if (!Credential->RootCertStore) {
		Error = GetLastError();
		return Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND;
	}

	return TRUE;
}
