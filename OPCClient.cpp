// TP1 - Brener and Daniel
//
// This client is based on the Simple OPC Client project, with additional funtionalities regarding a integration with a socket client
//
//
// The original Simple OPC Client code can still be found (as of this date)
// in
//        http://pgras.home.cern.ch/pgras/OPCClientTutorial/
//

#include <atlbase.h>    // required for using the "_T" macro
#include <iostream>
#include <thread>
#include <conio.h>
#include <variant>
#include <comdef.h>
#include <mutex>

#include "opcda.h"
#include "opcerror.h"
#include "OPCClient.h"
#include "SOCAdviseSink.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"
#include "Socket.h"

using namespace std;

#define OPC_SERVER_NAME L"Matrikon.OPC.Simulation.1"
#define VT VT_R4


// Global variables

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat (_T("OPCSTMFORMATDATATIME"));

struct OPCItem {
	char* PV;
	char* Tag;
	wchar_t* ID;
	int hClient;
	OPCHANDLE hServerItem;
};

OPCItem PRE_HEATING = { "Temperatura pré aquecimento", "Random.Real4", L"Random.Real4", 1, 0 };
OPCItem HEATING = { "Temperatura aquecimento", "Saw-toothed Waves.Real4", L"Saw-toothed Waves.Real4", 2, 0 };
OPCItem SOAK = { "Temperatura encharque", "Triangle Waves.Real4", L"Triangle Waves.Real4", 3, 0 };
OPCItem FLOW = { "Vazão", "Square Waves.Real4", L"Square Waves.Real4", 4, 0 };
OPCItem PRE_HEATING_SETPOINT = { "Setpoint pré aquecimento", "Bucket Brigade.Real8", L"Bucket Brigade.Real8", 5, 0 };
OPCItem HEATING_SETPOINT = { "Setpoint aquecimento", "Bucket Brigade.Real4", L"Bucket Brigade.Real4", 6, 0 };
OPCItem SOAK_SETPOINT = { "Setpoint encharque", "Bucket Brigade.Int4", L"Bucket Brigade.Int4", 7, 0 };

VARIANT PreHeatingSP; //to store the read value
VARIANT HeatingSP; //to store the read value
VARIANT SoakSP; //to store the read value


//////////////////////////////////////////////////////////////////////
// Read the value of an item on an OPC server. 
//
void main(void)
{
	IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
	IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface

	OPCHANDLE hServerGroup; // server handle to the group
	OPCHANDLE hServerItem;  // server handle to the item

	char buf[100];
	char address[30] = { NULL };

	setlocale(LC_ALL, "Portuguese");
	printf("Digite o endereço IP do computador de processo:\n");
	scanf("%s", &address);

	// Have to be done before using microsoft COM library:
	printf("Inicializando ambiente COM...\n");
	CoInitializeEx(NULL, COINIT_MULTITHREADED); // Enables multithread

	// Let's instantiante the IOPCServer interface and get a pointer of it:
	printf("Instanciando simulador de simulação OPC Matrikon...\n");
	pIOPCServer = InstantiateServer(OPC_SERVER_NAME);
	
	// Add the OPC group the OPC server and get an handle to the IOPCItemMgt
	//interface:
	printf("Adicionando grupo...\n");
	AddTheGroup(pIOPCServer, pIOPCItemMgt, hServerGroup);

	// Add the OPC items
    size_t m;
	printf("Adicionado itens ao grupo...\n");
	AddTheItem(pIOPCItemMgt, hServerItem);
	
	// Initializes all variants:
	PreHeatingSP.vt = VT_R8;
	HeatingSP.vt = VT_R4;
	SoakSP.vt = VT_I4;
	PreHeatingSP.dblVal = 0.0;
	HeatingSP.fltVal = 0.0;
	SoakSP.intVal = 0;
	
	int bRet;
	MSG msg;
	    
	// Establish a callback asynchronous read by means of the IOPCDaraCallback
	// (OPC DA 2.0) method. We first instantiate a new SOCDataCallback object and
	// adjusts its reference count, and then call a wrapper function to
	// setup the callback.
	IConnectionPoint* pIConnectionPoint = NULL; //pointer to IConnectionPoint Interface
	DWORD dwCookie = 0;
	SOCDataCallback* pSOCDataCallback = new SOCDataCallback();
	pSOCDataCallback->AddRef();


	printf("Estabelecendo conexão por IConnectionPoint callback...\n");
	SetDataCallback(pIOPCItemMgt, pSOCDataCallback, pIConnectionPoint, &dwCookie);

	// Change the group to the ACTIVE state so that we can receive the
	// server´s callback notification
	printf("Alterando o estado do grupo para ativo...\n");
    SetGroupActive(pIOPCItemMgt); 

	// Enter again a message pump in order to process the server´s callback
	// notifications, for the same reason explained before.
		
	printf("Aguardando notificações IOPCDataCallback...\n\n");
	int key = 0;
	
	std::thread t1(ManageNotifications, bRet, msg);	// Manages all notifications from DataCallback

	// The following thread runs the socket client
	std::thread t2(SocketMainThread, address, &pSOCDataCallback->PreHeatingValue, &pSOCDataCallback->HeatingValue, &pSOCDataCallback->SoakValue, &pSOCDataCallback->FlowValue, &pSOCDataCallback->PreHeatingSPValue, &pSOCDataCallback->HeatingSPValue, &pSOCDataCallback->SoakSPValue, &key);
	
	char KeyboardEntry = NULL;
	// Loop handles keyboard entries and calls syncronous functions
	while (true) {
		KeyboardEntry = _getch();
		if (KeyboardEntry == 's' || KeyboardEntry == 'S') {
			key = 1;
			while (key == 1);
			PreHeatingSP.dblVal = pSOCDataCallback->PreHeatingSPValue;
			HeatingSP.fltVal = pSOCDataCallback->HeatingSPValue;
			SoakSP.intVal = pSOCDataCallback->SoakSPValue;
			WriteItem(pIOPCItemMgt);
			printf("\n\nSetpoints atualizados \n \n");
			Sleep(1000);
		}
		if (KeyboardEntry == 'f' || KeyboardEntry == 'F') {
			printf("\n\nFinalizando aplicação... \n \n");
			key = 2;
			break;
		}
	}
	
	t1.join();
	t2.join();
	// Cancel the callback and release its reference
	printf("Cancelando notificações IOPCDataCallback...\n");
    CancelDataCallback(pIConnectionPoint, dwCookie);
	//pIConnectionPoint->Release();
	pSOCDataCallback->Release();

	// Remove the OPC item:
	printf("Removendo itens OPC...\n");
	RemoveItem(pIOPCItemMgt, hServerItem);

	// Remove the OPC group:
	printf("Removendo o objeto do grupo OPC...\n");
    pIOPCItemMgt->Release();
	RemoveGroup(pIOPCServer, hServerGroup);

	// release the interface references:
	printf("Removendo o objeto do servidor OPC...\n");
	pIOPCServer->Release();

	//close the COM library:
	printf ("Liberando ambiente COM...\n");
	CoUninitialize();
}

////////////////////////////////////////////////////////////////////
// Instantiate the IOPCServer interface of the OPCServer
// having the name ServerName. Return a pointer to this interface
//
IOPCServer* InstantiateServer(wchar_t ServerName[])
{
	CLSID CLSID_OPCServer;
	HRESULT hr;

	// get the CLSID from the OPC Server Name:
	hr = CLSIDFromString(ServerName, &CLSID_OPCServer);
	_ASSERT(!FAILED(hr));


	//queue of the class instances to create
	LONG cmq = 1; // nbr of class instance to create.
	MULTI_QI queue[1] =
		{{&IID_IOPCServer,
		NULL,
		0}};

	//Server info:
	//COSERVERINFO CoServerInfo =
    //{
	//	/*dwReserved1*/ 0,
	//	/*pwszName*/ REMOTE_SERVER_NAME,
	//	/*COAUTHINFO*/  NULL,
	//	/*dwReserved2*/ 0
    //}; 

	// create an instance of the IOPCServer
	hr = CoCreateInstanceEx(CLSID_OPCServer, NULL, CLSCTX_SERVER,
		/*&CoServerInfo*/NULL, cmq, queue);
	_ASSERT(!hr);

	// return a pointer to the IOPCServer interface:
	return(IOPCServer*) queue[0].pItf;
}


/////////////////////////////////////////////////////////////////////
// Add group "Group1" to the Server whose IOPCServer interface
// is pointed by pIOPCServer. 
// Returns a pointer to the IOPCItemMgt interface of the added group
// and a server opc handle to the added group.
//
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt, 
				 OPCHANDLE& hServerGroup)
{
	DWORD dwUpdateRate = 0;
	OPCHANDLE hClientGroup = 0;

	// Add an OPC group and get a pointer to the IUnknown I/F:
    HRESULT hr = pIOPCServer->AddGroup(/*szName*/ L"Group1",
		/*bActive*/ FALSE,
		/*dwRequestedUpdateRate*/ 1000,
		/*hClientGroup*/ hClientGroup,
		/*pTimeBias*/ 0,
		/*pPercentDeadband*/ 0,
		/*dwLCID*/0,
		/*phServerGroup*/&hServerGroup,
		&dwUpdateRate,
		/*riid*/ IID_IOPCItemMgt,
		/*ppUnk*/ (IUnknown**) &pIOPCItemMgt);
	_ASSERT(!FAILED(hr));
}



//////////////////////////////////////////////////////////////////
// Add the Item HEATING_ID to the group whose IOPCItemMgt interface
// is pointed by pIOPCItemMgt pointer. Return a server opc handle
// to the item.
 
void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem)
{
	HRESULT hr;

	// Array of items to add:
	OPCITEMDEF ItemArray[] =
	{{
		/*szAccessPath*/ L"",
		/*szItemID*/ PRE_HEATING.ID,
		/*bActive*/ TRUE,
		/*hClient*/ PRE_HEATING.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ HEATING.ID,
		/*bActive*/ TRUE,
		/*hClient*/ HEATING.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ SOAK.ID,
		/*bActive*/ TRUE,
		/*hClient*/ SOAK.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ FLOW.ID,
		/*bActive*/ TRUE,
		/*hClient*/ FLOW.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ PRE_HEATING_SETPOINT.ID,
		/*bActive*/ TRUE,
		/*hClient*/ PRE_HEATING_SETPOINT.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ HEATING_SETPOINT.ID,
		/*bActive*/ TRUE,
		/*hClient*/ HEATING_SETPOINT.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ SOAK_SETPOINT.ID,
		/*bActive*/ TRUE,
		/*hClient*/ SOAK_SETPOINT.hClient,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	}
	};

	//Add Result:
	OPCITEMRESULT* pAddResult=NULL;
	HRESULT* pErrors = NULL;

	// Add an Item to the previous Group:
	hr = pIOPCItemMgt->AddItems(7, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK){
		printf("Failed call to AddItems function. Error code = %x\n", hr);
		exit(0);
	}

	// Server handle for the added item:
	hServerItem = pAddResult[0].hServer;
	PRE_HEATING.hServerItem = pAddResult[0].hServer;
	HEATING.hServerItem = pAddResult[1].hServer;
	SOAK.hServerItem = pAddResult[2].hServer;
	FLOW.hServerItem = pAddResult[3].hServer;
	PRE_HEATING_SETPOINT.hServerItem = pAddResult[4].hServer;
	HEATING_SETPOINT.hServerItem = pAddResult[5].hServer;
	SOAK_SETPOINT.hServerItem = pAddResult[6].hServer;


	// release memory allocated by the server:
	CoTaskMemFree(pAddResult->pBlob);

	CoTaskMemFree(pAddResult);
	pAddResult = NULL;

	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

// Write values on OPC items syncronously 
void WriteItem(IUnknown* pGroupIUnknown)
{
	// value of the item:
	DWORD dwCount = 3;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**)&pIOPCSyncIO);

	// write the item value from the device:
	OPCHANDLE hServerItems[] = { PRE_HEATING_SETPOINT.hServerItem, HEATING_SETPOINT.hServerItem, SOAK_SETPOINT.hServerItem };
	VARIANT SetPoints[] = { PreHeatingSP, HeatingSP, SoakSP };
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Write(dwCount, hServerItems, SetPoints, &pErrors);
	_com_error err(hr);
	LPCTSTR errMsg = err.ErrorMessage();
	_ASSERT(!hr);


	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem)
{
	// server handle of items to remove:
	OPCHANDLE hServerArray[1];
	hServerArray[0] = hServerItem;
	
	//Remove the item:
	HRESULT* pErrors; // to store error code(s)
	HRESULT hr = pIOPCItemMgt->RemoveItems(1, hServerArray, &pErrors);
	_ASSERT(!hr);

	//release memory allocated by the server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

////////////////////////////////////////////////////////////////////////
// Remove the Group whose server handle is hServerGroup from the server
// whose IOPCServer interface is pointed by pIOPCServer
//
void RemoveGroup (IOPCServer* pIOPCServer, OPCHANDLE hServerGroup)
{
	// Remove the group:
	HRESULT hr = pIOPCServer->RemoveGroup(hServerGroup, FALSE);
	if (hr != S_OK){
		if (hr == OPC_S_INUSE)
			printf ("Failed to remove OPC group: object still has references to it.\n");
		else printf ("Failed to remove OPC group. Error code = %x\n", hr);
		exit(0);
	}
}

void ManageNotifications(int bRet, MSG msg) {
	bRet = GetMessage(&msg, NULL, 0, 0);
	if (!bRet) {
		printf("Failed to get windows message! Error code = %d\n", GetLastError());
		exit(0);
	}
}
