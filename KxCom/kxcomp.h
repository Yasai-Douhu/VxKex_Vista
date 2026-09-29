#pragma once

#include <KexComm.h>
#include <KexDll.h>
#include <KxCom.h>

EXTERN PKEX_PROCESS_DATA KexData;
VOID ComConfigureInnoShellLink(REFCLSID ClassId, IUnknown *Object);

KXCOMAPI HRESULT STDMETHODCALLTYPE CActivationFactory_QueryInterface(
	IN	IActivationFactory	*This,
	IN	REFIID				RefIID,
	OUT	PPVOID				Object);
