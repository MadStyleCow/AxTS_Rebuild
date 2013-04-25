#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#define _USE_MATH_DEFINES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <process.h>
#include <queue>
#include <string>
#include <qhash.h>
#include <sstream>
#include <math.h>
#include <qstring.h>
#include <qstringlist.h>
#include "include/public_errors.h"
#include "include/public_errors_rare.h"
#include "include/public_definitions.h"
#include "include/public_rare_definitions.h"
#include "include/ts3_functions.h"
#include "include/ts3plugin.h"
#include "include/parser.h"
#include "include/relative_pos.h"

#include <qmatrix4x4.h>
#include <qvector3d.h>
using namespace std;

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION	19

#define PATH_BUFSIZE		512
#define COMMAND_BUFSIZE		128
#define INFODATA_BUFSIZE	128
#define SERVERINFO_BUFSIZE	256
#define CHANNELINFO_BUFSIZE	512
#define RETURNCODE_BUFSIZE	128
#define SERVER_NAME			L"."
#define PIPE_NAME			L"a2ts"
#define FULL_PIPE_NAME		L"\\\\" SERVER_NAME L"\\pipe\\" PIPE_NAME
#define BUFFER_SIZE			512
#define MAX_VOICE_RANGE		100
#define MAX_LW_RANGE		5000
#define MAX_SW_RANGE		1000

// Plugin variables
static char* pluginID = NULL;
uint64 connectionHandlerID = 0;
anyID myId = 0;
char* chname[] = {"PvP_WOG","RT",""};

// System variables
DWORD dwError = ERROR_SUCCESS;
HANDLE clientPipe;
HANDLE receiverThreadHndl	= NULL;
HANDLE handlingThreadHndl	= NULL;
HANDLE senderThreadHndl		= NULL;
HANDLE timerThreadHndl      = NULL;
queue<string> incomingMessages;
queue<string> outgoingMessages;
QHash<anyID, relativePOS_Struct> relativePosHash;
QHash<anyID,argsComOTH> players;
relativePOS_Struct *relativePos;
uint64 newcid = 9999;
uint64 oldcid = 9999;
argsComPOS *self; 
argsComPOS *oldSelf;
argsComOTH *other; 
argsComMIN *miniOther; 
QMatrix4x4 trans_matrix;

// Changeable variables
BOOL inRt = 0;
BOOL stopRequested = FALSE;
BOOL connected = 0;
BOOL talking = FALSE;
BOOL vadEnabled = FALSE;
BOOL timerReset = FALSE;
BOOL recalcRequired = FALSE;
BOOL pipeOpen;

/* 
 * ALL CUSTOM FUNCTIONS HAVE SPECIFIC PREFIXES INDICATING WHAT DO THEY DO *
 * msg_				- Messaging;
 * ipc_				- Inter-process communication handling;
 * pos_				- Positioning;
 * vPos_			- Voice positioning;
 * rPos_			- Radio positioning;
 * ts3plugin_		- TS3 Callbacks and Required functions
 * ts3Functions.	- TS3 Client Library functions
 * hlp_				- Helper functions
 * chnl_            - Channel movement functions
 * prs_				- Parsing & Processing of received data
 * vce_				- Voice & Volume calculation and alterations
 */

void ipc_pipeConnect();
void ipc_receiveCommand(void* pArguments);
void ipc_sendCommand(void* pArguments);
void ipc_handlingReceivedCommands(void* pArguments);

void chnl_moveToRt();
void chnl_moveFromRt();

void msg_generateOTH(string &result);
void msg_generateREQ(string &result);
void msg_generateMIN(string &result);

void pos_client(anyID idClient);
void pos_self();
void vPos_logic(anyID &idClient);
void vPos_infantry(anyID &idClient);
void vPos_vehicle(anyID &idClient);
void rPos_swLogic(anyID &idClient);
void rPos_lwLogic(anyID &idClient);
void rPos_clientSW(anyID &idClient, int &channelPos);
void rPos_clientLW(anyID &idClient, int &channelPos);

double hlp_getDistance(float x1, float y1, float z1, float x2, float y2, float z2);
void hlp_muteClient(anyID &idClient);
void hlp_unmuteClient(anyID &idClient);
void hlp_sendPluginCommand(string &commandText, anyID &idClient, BOOL isBroadcast);
BOOL hlp_majorSelfDataChange();
BOOL hlp_checkVad();
void hlp_enableVad();
void hlp_disableVad();
int hlp_getChannel(anyID idClient, float clientArray[], float selfArray[]);
void hlp_timerThread(void* pArguments);
void hlp_setMetaData(string data);
string hlp_generateMetaData();

void prs_commandText(string &commandText, anyID &idClient);
void prs_parseREQ(anyID &idClient);
void prs_parseMIN(anyID &idClient);
void prs_parseOTH(anyID &idClient);
void prs_parsePOS();

void RadioNoiseDSP(float slevel, short * samples, int sampleCount);

/*********************************** Required functions START ************************************/

const char* ts3plugin_name() {
	return "A2TS Rebuild x32";
}

const char* ts3plugin_version() {
    return "v0.5";
}

int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "MSC && Kolun && Swapp";
}

const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "A2TS Rebuild: This plugin will (not) work.";
}

const char* ts3plugin_infoTitle() 
{
	return "Plugin info";
}

void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

void ts3plugin_freeMemory(void* data)
{
	free(data);
}

int ts3plugin_init()
{
	/* Plugin init code goes here */
	printf("PLUGIN: Init\n");

	int errorCode = 0;

	self = new argsComPOS();
	oldSelf = new argsComPOS();
	other = new argsComOTH();
	miniOther = new argsComMIN();
	relativePos = new relativePOS_Struct();

	errorCode = connectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
	if(connectionHandlerID != 0)
	{
		int connectionState;
		errorCode = ts3Functions.getConnectionStatus(connectionHandlerID, &connectionState);
		if(errorCode == ERROR_ok)
		{
			if(connectionState == STATUS_CONNECTION_ESTABLISHED)
			{
				if(ts3Functions.getClientID(connectionHandlerID, &myId) != ERROR_ok)
				{
					printf("PLUGIN: Failed to receive client ID.\n"); 
				}
				else
				{
					printf("PLUGIN: Current client ID: %d\n", myId);
				}

				connected = TRUE;

				// Set client metadata, publically declaring that we are using this plug-in.
				hlp_setMetaData(hlp_generateMetaData());
			}
		}
	}
	else
	{
		printf("PLUGIN: Not connected to a server. No server connection handler or client ID available.\n");
		connected = FALSE;
		inRt = FALSE;
	}
	
	// Initialiaze a named-pipe listener thread.
	if(receiverThreadHndl == NULL)
	{
		printf("PLUGIN: Receiver handle is unassigned. Assigning..\n");
		receiverThreadHndl = (HANDLE)_beginthread(ipc_receiveCommand, 0, NULL);
	}
	else
	{
		printf("PLUGIN: Receiver handle already assigned. \n");
		if(GetThreadId(receiverThreadHndl) != NULL)
		{
			printf("PLUGIN: Thread id: %d\n", GetThreadId(receiverThreadHndl));
		}
		else
		{
			printf("PLUGIN: Couldn't start receiver thread.\n");
			return 1;
		}
	}

/*	//Initialize a named-pipe sender thread.
	if(senderThreadHndl == NULL)
	{
		printf("PLUGIN: Sender handle is unassigned. Assigning..\n");
		senderThreadHndl = (HANDLE)_beginthread(ipc_sendCommand, 0, NULL);
	}
	else
	{
		printf("PLUGIN: Sender handle already assigned. \n");
		if(GetThreadId(senderThreadHndl) != NULL)
		{
			printf("PLUGIN: Thread id: %d\n", GetThreadId(senderThreadHndl));
		}
		else
		{
			printf("PLUGIN: Couldn't start sender thread.\n");
			return 1;
		}
	}*/

	// Initialize an timer thread
	if(timerThreadHndl == NULL)
	{
		printf("PLUGIN: Timer handle is unassigned. Assigning..\n");
		timerThreadHndl = (HANDLE)_beginthread(hlp_timerThread, 0, NULL);
	}
	else
	{
		printf("PLUGIN: Timer handle already assigned. \n");
		if(GetThreadId(timerThreadHndl) != NULL)
		{
			printf("PLUGIN: Thread id: %d\n", GetThreadId(timerThreadHndl));
		}
		else
		{
			printf("PLUGIN: Couldn't start timer thread.\n");
			return 1;
		}
	}

	// Initialize an incoming message handling thread
	if(handlingThreadHndl == NULL)
	{
		printf("PLUGIN: Handling handle is unassigned. Assigning..\n");
		handlingThreadHndl = (HANDLE)_beginthread(ipc_handlingReceivedCommands, 0, NULL);
	}
	else
	{
		printf("PLUGIN: Handling handle already assigned. \n");
		if(GetThreadId(handlingThreadHndl) != NULL)
		{
			printf("PLUGIN: Thread id: %d\n", GetThreadId(handlingThreadHndl));
		}
		else
		{
			printf("PLUGIN: Couldn't start handling thread.\n");
			return 1;
		}
	}

	printf("PLUGIN: Completed init().\n");

    return 0;  /* 0 = success, 1 = failure */
}

void ts3plugin_shutdown()
{
	printf("PLUGIN: Shutdown called.\n");

	// Remove the player from RT
	if(inRt == TRUE)
	{
		chnl_moveFromRt();
	}

	// Request thread stop.
	stopRequested = TRUE;

	// Await timer thread stop
	while(timerThreadHndl != INVALID_HANDLE_VALUE)
	{
		printf("PLUGIN: Awaiting timer thread shutdown.\n");
		Sleep(100);
	}

	printf("PLUGIN: Timer thread shutdown confirmed.\n");

	// Await handling thread stop
	CancelSynchronousIo(handlingThreadHndl);
	while(handlingThreadHndl != INVALID_HANDLE_VALUE)
	{
		printf("PLUGIN: Awaiting handling thread shutdown.\n");
		Sleep(100);
	}

	printf("PLUGIN: Handling thread shutdown confirmed.\n");

/*	// Await sender thread stop
	CancelSynchronousIo(senderThreadHndl);
	while(senderThreadHndl != INVALID_HANDLE_VALUE)
	{
		printf("PLUGIN: Awaiting sender thread shutdown.\n");
		Sleep(100);
	}
	printf("PLUGIN: Sender thread shutdown confirmed.\n");*/

	// Await receiver thread stop
	CancelSynchronousIo(receiverThreadHndl);
	while(receiverThreadHndl != INVALID_HANDLE_VALUE)
	{
		printf("PLUGIN: Awaiting receiver thread shutdown.\n");
		Sleep(100);
	}
	printf("PLUGIN: Receiver thread shutdown confirmed.\n");

	// Await pipe closure
	if(clientPipe != INVALID_HANDLE_VALUE && clientPipe != NULL)
	{
		CloseHandle(clientPipe);
	}

	// Clear client metadata.
	hlp_setMetaData("");
	
	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}

/*********************************** Required functions END ************************************/

/********************************** TS3 Callbacks **********************************************/

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber)
{
	// This is called when a user joins a server (if the plug-in was launched while he was not connected) or rejoins/joins another server.
	if(newStatus == STATUS_CONNECTION_ESTABLISHED)
	{
		connectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
		if(connectionHandlerID)
		{
			if(ts3Functions.getClientID(connectionHandlerID, &myId) != ERROR_ok)
			{
				printf("PLUGIN: Failed to receive client ID.\n"); 
			}
			else
			{
				printf("PLUGIN: Connected to a server.\n");
				printf("PLUGIN: Current client ID: %d\n", myId);

				// Set 3D settings for the server

				int errorCode = ts3Functions.systemset3DSettings(connectionHandlerID, 1.0f, 10.0f);
				if(errorCode != ERROR_ok)
				{
					printf("PLUGIN: Failed to set system 3D settings. Error code %d\n", errorCode);
				}
				else
				{
					printf("PLUGIN: System 3D settings set.\n");
				}
			}
			// Clear any awaiting messages. (If we for example disconnect and reconnect during game).
			while(!incomingMessages.empty())
			{
				incomingMessages.pop();
			}
			connected = TRUE;

			// Set client metadata, publically declaring that we are using this plug-in.
			hlp_setMetaData(hlp_generateMetaData());
		}
		else
		{
			printf("PLUGIN: Failure to receive new server connection handler.\n");
			// Unload plug-in.
			ts3plugin_shutdown();
		}
	}
	else
	{
		connected = FALSE;
		inRt = FALSE;
	}
}

void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID, const char* pluginName, const char* pluginCommand)
{
	if(connected == TRUE && inRt == TRUE && serverConnectionHandlerID == connectionHandlerID)
	{
		if(strcmp(pluginName, "a2ts_rebuild") != 0)
		{
			printf("PLUGIN: Plugin command event failure.\n");
		}
		else
		{
			QString commandText(pluginCommand);
			QStringList separatedCommandText = commandText.split("@");

			if(separatedCommandText.length() != 2)
			{
				printf("PLUGIN: Invalid command.\n");
				return;
			}
			else
			{
				anyID idClient = (anyID)separatedCommandText[0].toInt();
				// We want to ignore commands coming from ourselves.
				if(myId != idClient)
				{
					string parseCommandText(separatedCommandText[1].toLocal8Bit().constData());
					prs_commandText(parseCommandText, idClient);
				}
			}
		}
	}
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage)
{
	if(serverConnectionHandlerID == connectionHandlerID)
		{
		if(clientID == myId)
		{
			char* result;
			if(ts3Functions.getChannelVariableAsString(connectionHandlerID, newChannelID, CHANNEL_NAME, &result) == ERROR_ok)
			{
				if(strcmp(result, "RT") != 0)
				{
					printf("PLUGIN: Switched to a non-RT channel.\n");
					inRt = FALSE;
				}
				else
				{
					printf("PLUGIN: Switched to RT channel.\n");
					newcid = newChannelID;
					inRt = TRUE;
				}
				ts3Functions.freeMemory(result);
			}
		}
		else
		{
			if(inRt == TRUE)
			{
				if(newChannelID == newcid)
				{
					hlp_muteClient(clientID);
				}
				else
				{
					hlp_unmuteClient(clientID);
					if(players.contains(clientID))
					{
						players.remove(clientID);
					}
				}
			}
		}
	}
}

void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID)
{
	if(inRt == TRUE && clientID == myId && serverConnectionHandlerID == connectionHandlerID)
	{
		string minMessage;
		msg_generateMIN(minMessage);
		hlp_sendPluginCommand(minMessage, clientID, TRUE);
	}
}

void ts3plugin_onCustom3dRolloffCalculationClientEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance , float* volume ) // AWAITING REWRITE
{
	if(inRt == TRUE && players.contains(clientID) && serverConnectionHandlerID == connectionHandlerID)
	{
		if(players[clientID].TAN == 1 && players[clientID].hearableKV != -1)
		{
			// Shortwave
			float swVolArray[] = {self->kvVol0, self->kvVol1, self->kvVol2, self->kvVol3};

			*volume = swVolArray[players[clientID].hearableKV] / 100;
		}
		else if(players[clientID].TAN == 2 && players[clientID].hearableDV != -1)
		{
			// Longwave
			float lwVolArray[] = {self->dvVol0, self->dvVol1, self->dvVol2, self->dvVol3};

			*volume = lwVolArray[players[clientID].hearableDV] / 100;
		}
		else
		{
			// Voice
			float calculatedVolume;

			switch(players[clientID].Mode)
			{
			case 0:
				// Whisper
				calculatedVolume = 1.0f - distance * 0.3f; // ~3,33 meters of hearing range.
				break;
			case 1:
				// Normal
				calculatedVolume = 1.0f - distance * 0.1f; // ~ 10 meters of hearing range.
				break;
			case 2:
				// Screaming
				calculatedVolume = 1.0f - distance * 0.01f; // ~ 100 meters of hearing range.
				break;
			}

			if(calculatedVolume < 0.0f)
			{
				*volume = 0.0f;
			}
			else
			{
				*volume = calculatedVolume;
			}
		}
	}
}

void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
	if(inRt == TRUE && players.contains(clientID) && serverConnectionHandlerID == connectionHandlerID)
	{
		if((players[clientID].TAN == 1 || players[clientID].TAN == 2) && (players[clientID].hearableKV != -1 || players[clientID].hearableDV != -1))
		{
			RadioNoiseDSP(1.00f, samples, sampleCount);
		}
	}
}

void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data)
{
	if(connected == TRUE && serverConnectionHandlerID == connectionHandlerID)
	{
		char* metaData = "No plug-in detected.";

		switch(type) 
		{
			case PLUGIN_CLIENT:
				if(ts3Functions.getClientVariableAsString(connectionHandlerID, (anyID)id, CLIENT_META_DATA, &metaData) != ERROR_ok)
				{
					printf("PLUGIN: Failure querying metadata.\n");
					return;
				}
				break;
			default:
				data = NULL;  /* Ignore */
				return;
		}
		
		if(*metaData == '\0')
		{
			metaData = "No plug-in detected.";
		}

		*data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));
		snprintf(*data, INFODATA_BUFSIZE, "[I]\%s\[/I]", metaData);
		ts3Functions.freeMemory(metaData);
	}
}

/********************************** TS3 Callback END *******************************************/

/********************************** IPC Implementation *****************************************/
void ipc_pipeConnect()
{
	// Try to open the named pipe identified by the pipe name.
	while (stopRequested != TRUE) 
    {
        clientPipe = CreateFile( 
            FULL_PIPE_NAME,                 // Pipe name 
            GENERIC_READ | GENERIC_WRITE,   // Read and write access
            0,                              // No sharing 
            NULL,                           // Default security attributes
            OPEN_EXISTING,                  // Opens existing pipe
            0,                              // Default attributes
            NULL                            // No template file
            );

		if(clientPipe != INVALID_HANDLE_VALUE)
		{
			printf("PLUGIN: Connected to a server pipe.\n");
			// Update metadata to indicate that we are connected.
			pipeOpen = TRUE;
			hlp_setMetaData(hlp_generateMetaData());
			// Set the read mode and the blocking mode of the named pipe.
			DWORD dwMode = PIPE_READMODE_MESSAGE;
			if (!SetNamedPipeHandleState(clientPipe, &dwMode, NULL, NULL))
			{
				dwError = GetLastError();
				printf("PLUGIN: SetNamedPipeHandleState failed w/err 0x%08lx\n", dwError);
			}
			break;
		}
		else
		{
			// Update metadata to indicate that we are disconnected
			// Condition to prevent spamming updates of metadata.
			if(pipeOpen != FALSE)
			{
				pipeOpen = FALSE;
				hlp_setMetaData(hlp_generateMetaData());
			}
			// Wait before re-trying.
			Sleep(500);
		}
    }
}

void ipc_receiveCommand(void* pArguments)
{
	// Call pipe connection function.

		ipc_pipeConnect();

		BOOL fFinishRead = FALSE;
		char chResponse[BUFFER_SIZE];
		DWORD cbResponse, cbRead;
		cbResponse = sizeof(chResponse) -1;
		int errCode;

		while(stopRequested != TRUE)
		{
			if(connected == TRUE)
			{
				fFinishRead = ReadFile(
					clientPipe,             // Handle of the pipe
					chResponse,             // Buffer to receive the reply
					cbResponse,             // Size of buffer in bytes
					&cbRead,                // Number of bytes read 
					NULL                    // Not overlapped 
					);

				if (fFinishRead)
				{
					chResponse[cbRead] = '\0';
					incomingMessages.push(chResponse);	
				}
				else
				{
					errCode = GetLastError();
					if(errCode == 109)
					{
						CloseHandle(clientPipe);
						ipc_pipeConnect();
					}
					else if(errCode == 995)
					{
						printf("PLUGIN: IO Operation aborted.\n");
					}
					else
					{
						printf("PLUGIN: Read failed. Error code: %d\n",GetLastError());
					}
				}
			}
		}
		CloseHandle(receiverThreadHndl);
		receiverThreadHndl = INVALID_HANDLE_VALUE;
}

void ipc_sendCommand(void* pArguments)
{
	while(clientPipe == INVALID_HANDLE_VALUE)
	{
		if(stopRequested == TRUE)
		{
			break;
		}
		else
		{
			Sleep(500);
		}
	}

	printf("PLUGIN: Connected to a server pipe.\n");

	while(stopRequested != TRUE)
	{
		if(inRt == TRUE && !outgoingMessages.empty())
		{
			const char* chRequest = outgoingMessages.front().c_str();
			outgoingMessages.pop();
			DWORD cbWritten;

			printf("chRequest is %s\n", chRequest);

			if (!WriteFile(
				clientPipe,											// Handle of the pipe
				chRequest,											// Message to be written
				sizeof(chRequest),									// Number of bytes to write
				&cbWritten,											// Number of bytes written
				NULL												// Not overlapped
				))
			{
				dwError = GetLastError();
				printf("PLUGIN: WriteFile to pipe failed w/err 0x%08lx\n", dwError);
			}
		}
		else
		{
			Sleep(100);
		}
	}

	CloseHandle(senderThreadHndl);
	senderThreadHndl = INVALID_HANDLE_VALUE;
}

void ipc_handlingReceivedCommands(void* pArguments)
{
	while(stopRequested != TRUE)
	{
		if(connected == TRUE && !incomingMessages.empty())
		{
			// Get a command from the queue.
			string commandText = incomingMessages.front();
			incomingMessages.pop();

			// Pass it on for parsing.
			prs_commandText(commandText, myId);
		}
		else
		{
			Sleep(100);
		}
	}

	CloseHandle(handlingThreadHndl);
	handlingThreadHndl = INVALID_HANDLE_VALUE;
} 
/********************************** IPC Implementation END *************************************/

/********************************** Channel Movement *******************************************/
void chnl_moveToRt()
{
	// Move user to RT channel.
	int errorCode;
	errorCode = ts3Functions.getChannelIDFromChannelNames(connectionHandlerID, chname, &newcid);
	if(errorCode == ERROR_ok)
	{
		if(newcid != NULL)
		{
			anyID *clientList;
			errorCode = ts3Functions.getChannelOfClient(connectionHandlerID, myId, &oldcid);
			if(errorCode != ERROR_ok)
				printf("PLUGIN: Failed to get channel of client.\n");
			
			errorCode = ts3Functions.requestClientMove(connectionHandlerID, myId, newcid, "4321", 0);
			if(errorCode != ERROR_ok)
				printf("PLUGIN: Failed to requet client move.\n");
			
			errorCode = ts3Functions.getChannelClientList(connectionHandlerID, newcid, &clientList);
			if(errorCode != ERROR_ok)
				printf("PLUGIN: Failed to get channel client list.\n");
			else
			{
				errorCode = ts3Functions.requestMuteClients(connectionHandlerID, clientList, 0);
				ts3Functions.freeMemory(clientList);
				if(errorCode != ERROR_ok)
					printf("PLUGIN: Failed to mute clients in channel.\n");
			}
			inRt = TRUE;
		}
		else
		{
			printf("PLUGIN: No RT channel found.\n");
		}
	}
	else
	{
		printf("PLUGIN: Failed to get RT channel.\n");
	}
}

void chnl_moveFromRt()
{
	if(inRt == TRUE)
	{
		int errorCode;
		anyID *clientList;

		// Unmute all clients in RT
		errorCode = ts3Functions.getChannelClientList(connectionHandlerID, newcid, &clientList);
		if(errorCode == ERROR_ok)
		{
			errorCode = ts3Functions.requestUnmuteClients(connectionHandlerID, clientList, ERROR_ok);
			if(errorCode == ERROR_ok)
			{
				printf("PLUGIN: Unmuted all clients in RT channel.\n");
			}
			else
			{
				printf("PLUGIN: Failed unmuting all clients in RT.\n");
			}
			ts3Functions.freeMemory(clientList);
		}
		else
		{
			printf("PLUGIN: Failed to get all clients in RT.\n");
		}

		// Move the player from RT
		errorCode = ts3Functions.requestClientMove(connectionHandlerID, myId, oldcid, "", 0);
		if(errorCode == ERROR_ok)
		{
			printf("PLUGIN: Moved user back to old channel.\n");
			inRt = FALSE;
		}
		else
		{
			printf("PLUGIN: Failed to move user back to old channel. Trying to move to default channel.\n");

			uint64* allChannels;

			if(ts3Functions.getChannelList(connectionHandlerID, &allChannels) == ERROR_ok)
			{
				int isDefault = 0,
				i = 0;
				for(; (allChannels[i] != NULL) && (isDefault == 0); i++)
				if(ts3Functions.getChannelVariableAsInt(connectionHandlerID, allChannels[i], CHANNEL_FLAG_DEFAULT, &isDefault) != ERROR_ok )
				{
					printf("PLUGIN: Failed to check default flag of channel: %lld\n", allChannels[i]);
				}
				if(ts3Functions.requestClientMove(connectionHandlerID, myId, oldcid, "", 0) == ERROR_ok)
				{
					printf("PLUGIN: Moved user to default channel.\n");
					inRt = FALSE;
				}
				else
				{
					printf("PLUGIN: Failed to move user to default channel.\n");
					// Shutdown plug-in. Let the user sort it out himself.
					ts3plugin_shutdown();
				}
				ts3Functions.freeMemory(allChannels);
			}
			else
			{
				printf("PLUGIN: Failed to get channel list.\n");
			}
		}
	}
	else
		printf("PLUGIN: Client already not in RT.\n");
}

/********************************** Channel Movement END ***************************************/

/********************************** Message Generation *****************************************/
void msg_generateOTH(string &result)
{
	stringstream othStream;
	othStream << myId << "@[A2TS_CMD]OTH[/A2TS_CMD][A2TS_ARG]" 
		<< self->posX									<< ";" 
		<< self->posY									<< ";" 
		<< self->posZ									<< ";" 
		<< self->Dir									<< ";" 
		<< self->Mode									<< ";" 
		<< self->vehId.toLocal8Bit().constData()		<< ";" 
		<< self->isOut									<< ";" 
		<< self->kvChan0 								<< ";" 
		<< self->kvChan1 								<< ";" 
		<< self->kvChan2 								<< ";" 
		<< self->kvChan3								<< ";" 
		<< self->kvActive 								<< ";" 
		<< self->kvSide 								<< ";" 
		<< self->dvChan0 								<< ";" 
		<< self->dvChan1 								<< ";" 
		<< self->dvChan2 								<< ";" 
		<< self->dvChan3								<< ";" 
		<< self->dvActive 								<< ";" 
		<< self->dvSide 								<< ";" 
		<< self->TAN						<< ";[/A2TS_ARG]";
	printf("GENERATION: Generated OTH message.\n");
	result = othStream.str();
}

void msg_generateREQ(string &result)
{
	stringstream reqStream;
	reqStream << myId << "@[A2TS_CMD]REQ[/A2TS_CMD]";
	printf("GENERATION: Generated REQ message.\n");
	result = reqStream.str();
}

void msg_generateMIN(string &result)
{
	stringstream minStream;
	minStream << myId << "@[A2TS_CMD]MIN[/A2TS_CMD][A2TS_ARG]"
		<< self->posX		<< ";"
		<< self->posY		<< ";"
		<< self->posZ		<< ";"
		<< self->Dir		<< ";"
		<< self->Mode		<< ";"
		<< self->TAN 		<< ";[/A2TS_ARG]";
	printf("GENERATION: Generated MIN message.\n");
	result = minStream.str();
}

/********************************** Message Generation END *************************************/

/********************************** 3D Positioning *********************************************/

void pos_client(anyID idClient)
{
	if(players.contains(idClient))
	{
		switch(players[idClient].TAN)
		{
		case 0:
			// Voice
			vPos_logic(idClient);
			break;
		case 1:
			rPos_swLogic(idClient);
			// ShortWave
			break;
		case 2:
			rPos_lwLogic(idClient);
			// LongWave			
			break;
		}
	}
	else
	{
		printf("POSITIONING: UNKNOWN CLIENT!\n");
	}
}

void vPos_logic(anyID &idClient)
{
	// Voice
	if(hlp_getDistance(self->posX, self->posY, self->posZ, players[idClient].posX, players[idClient].posY, players[idClient].posZ) < MAX_VOICE_RANGE)
	{
		if(self->vehId != "0")
		{
			if(self->isOut == 1)
			{
				if(players[idClient].vehId != "0")
				{
					if(self->vehId == players[idClient].vehId)
					{
						hlp_unmuteClient(idClient);
						vPos_vehicle(idClient);
					}
					else
					{
						hlp_muteClient(idClient);
					}
				}
				else
				{
					hlp_unmuteClient(idClient);
					vPos_infantry(idClient);
				}
			}
			else
			{
				if(players[idClient].vehId != "0")
				{
					if(self->vehId == players[idClient].vehId)
					{
						hlp_unmuteClient(idClient);
						vPos_vehicle(idClient);
					}
					else
					{
						hlp_muteClient(idClient);
					}
				}
				else
				{
					hlp_muteClient(idClient);
				}
			}
		}
		else
		{
			if(players[idClient].vehId != "0")
			{
				hlp_muteClient(idClient);
			}
			else
			{
				hlp_unmuteClient(idClient);
				vPos_infantry(idClient);
			}
		}
	}
	else
	{
		hlp_muteClient(idClient);
	}
}

void rPos_swLogic(anyID &idClient)
{
	if(players[idClient].hearableKV != -1)
	{
		if(hlp_getDistance(self->posX, self->posY, self->posZ, players[idClient].posX, players[idClient].posY, players[idClient].posZ) < MAX_SW_RANGE) // Вынести отдельной функцией в hlp_
		{
			hlp_unmuteClient(idClient);
			rPos_clientSW(idClient, players[idClient].hearableKV);
		}
		else
		{
			hlp_muteClient(idClient);
		}
	}
	else
	{
		vPos_logic(idClient);
	}
}

void rPos_lwLogic(anyID &idClient)
{
	if(players[idClient].hearableDV != -1)
	{
		if(hlp_getDistance(self->posX, self->posY, self->posZ, players[idClient].posX, players[idClient].posY, players[idClient].posZ) < MAX_LW_RANGE)
		{
			hlp_unmuteClient(idClient);
			rPos_clientLW(idClient, players[idClient].hearableDV);
		}
		else
		{
			hlp_muteClient(idClient);
		}
	}
	else
	{
		vPos_logic(idClient);
	}
}

void pos_self()
{
	// Refresh own position, position all clients accordingly
	QVector3D pos = QVector3D(self->posX, self->posY, self->posZ);
	QMatrix4x4 tm;

	tm.translate(pos);
	tm.rotate(360 - self->Dir, 0, 0, 1);
	trans_matrix = tm.inverted();

	QHash<anyID, argsComOTH>::iterator i = players.begin();
	while(i != players.end())
	{
		pos_client(i.key());
		i++;
	}
}

void vPos_infantry(anyID &idClient)
{
	printf("DEBUG: VPOS INFANTRY\n");

	TS3_VECTOR clientPos;
	QVector3D rel_pos = trans_matrix.map(QVector3D(players[idClient].posX, players[idClient].posY, players[idClient].posZ));
	clientPos.x = rel_pos.x();
	clientPos.y = rel_pos.y();
	clientPos.z = rel_pos.z();

	int errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &clientPos);
	if(errorCode != ERROR_ok)
	{
		if(errorCode == ERROR_client_invalid_id)
		{
			printf("POSITIONING: Client %d does not exist, removing..\n", idClient);
			players.remove(idClient);
		}
		else
			printf("POSITIONING: Failed to position client %d. Error code %d\n", idClient, errorCode);
	}
	else
	{
		hlp_unmuteClient(idClient);
	}
}

void vPos_vehicle(anyID &idClient) // Узнать почему прыгает позиция.
{
	printf("DEBUG: VPOS VEHICLE\n");


	int errorCode = 0;
	TS3_VECTOR othPos;

	if(!relativePosHash.contains(idClient))
	{
		// Means we know of no such guy.
		// Calculate the coordinate drift.
		relativePos->xDrift = self->posX - players[idClient].posX;
		relativePos->yDrift = self->posY - players[idClient].posY;
		relativePos->zDrift = self->posZ - players[idClient].posZ;

		// Add the guy to the relative position hash.
		relativePosHash.insert(idClient, *relativePos);
	}

	// Set him to a required relative position.
	float x = self->posX + relativePosHash[idClient].xDrift;
	float y = self->posY + relativePosHash[idClient].yDrift;
	float z = self->posZ + relativePosHash[idClient].zDrift;
	
	QVector3D rel_pos = trans_matrix.map(QVector3D(x, y, z));
	othPos.x = rel_pos.x();
	othPos.y = rel_pos.y();
	othPos.z = rel_pos.z();

	errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &othPos);
	if(errorCode != ERROR_ok)
	{
		if(errorCode == ERROR_client_invalid_id)
		{
			printf("POSITIONING: Client %d does not exist, removing..\n", idClient);
			players.remove(idClient);
			relativePosHash.remove(idClient);
		}
		else
			printf("POSITIONING: Failed to position client %d. Error code %d\n", idClient, errorCode);
	}
	else
	{
		printf("POSITIONING: Remote client with ID %d positioned in 3D space.\n",idClient);
	}

	// Un-mute him.
	hlp_unmuteClient(idClient);
}
	
void rPos_clientSW(anyID &idClient, int &channelPos)
{
	int errorCode = 0;
	TS3_VECTOR othPos;
	int channelPosArray[] = {self->kvPos0, self->kvPos1, self->kvPos2, self->kvPos3};

	othPos.x = (channelPosArray[channelPos] -1) * 20;
	othPos.y = 0;
	othPos.z = 0;

	errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &othPos);
	if(errorCode != ERROR_ok)
	{
		printf("POSITIONING: Failed to set 3D position of remote client with ID %d . Error code: %d\n", idClient,errorCode);
	}
	else
	{
		printf("POSITIONING: Remote client with ID %d positioned in 3D space.\n",idClient);
	}
}

void rPos_clientLW(anyID &idClient, int &channelPos)
{
	int errorCode = 0;
	TS3_VECTOR othPos;
	int channelPosArray[] = {self->dvPos0, self->dvPos1, self->dvPos2, self->dvPos3};

	othPos.x = channelPosArray[channelPos] - 1 * 20;
	othPos.y = 0;
	othPos.z = 0;

	errorCode = ts3Functions.channelset3DAttributes(connectionHandlerID, idClient, &othPos);
	if(errorCode != ERROR_ok)
	{
		printf("POSITIONING: Failed to set 3D position of remote client with ID %d . Error code: %d\n", idClient,errorCode);
	}
	else
	{
		printf("POSITIONING: Remote client with ID %d positioned in 3D space.\n",idClient);
	}
}

/********************************** 3D Positioning END ****************************************/

/********************************** Helper Functions ******************************************/

double hlp_getDistance(float x1, float y1, float z1, float x2, float y2, float z2)
{
	return sqrt(pow((x2-x1),2.0f) + pow((y2-y1),2.0f) + pow((z2-z1),2.0f)); 
}

void hlp_muteClient(anyID &idClient)
{
	// Checks if given client is already muted. If not - mutes him. Else - does nothing.
	int clientMuted = -1;
	anyID clientArray[2] = {idClient,0};
	int errorCode = ts3Functions.getClientVariableAsInt(connectionHandlerID, idClient, CLIENT_IS_MUTED, &clientMuted);
	if(errorCode != ERROR_ok)
	{
		printf("DEBUG: MUTE FAILED FOR CLIENT %d\n", idClient);
	}
	if(clientMuted == 0)
	{
		 ts3Functions.requestMuteClients(connectionHandlerID, clientArray, NULL); 
		if(errorCode != ERROR_ok)
		{
			printf("PLUGIN: Failed to mute client with ID %d. Error code: %d\n", idClient, errorCode);
		}
		else
		{
			printf("PLUGIN: Client with ID %d muted.\n",idClient);
		}
	}
}

void hlp_unmuteClient(anyID &idClient)
{
	// Checks if client is muted. If he is - unmutes him. Else - does nothing.
	anyID clientArray[2] = {idClient,0};
	int clientMuted;

	int errorCode = ts3Functions.getClientVariableAsInt(connectionHandlerID, idClient, CLIENT_IS_MUTED, &clientMuted);
	if(errorCode != ERROR_ok)
	{
		printf("DEBUG: UNMUTE FAILED FOR CLIENT %d\n", idClient);
	}
	else
	{
		if(clientMuted == 1)
		{
			errorCode = ts3Functions.requestUnmuteClients(connectionHandlerID, clientArray, NULL); 
			if(errorCode != ERROR_ok)
			{
				printf("PLUGIN: Failed to unmute client with ID %d. Error code: %d\n", idClient, errorCode);
			}
			else
			{
				printf("PLUGIN: Remote client with ID %d unmuted.\n",idClient);
			}
		}
	}
}

BOOL hlp_checkVad()
{
	char* vad; // Is "true" or "false"
	if(ts3Functions.getPreProcessorConfigValue(connectionHandlerID, "vad", &vad) == ERROR_ok)
	{
		if(strcmp(vad,"true") == 0)
		{
			ts3Functions.freeMemory(vad);
			return TRUE;
		}
		else
		{
			ts3Functions.freeMemory(vad);
			return FALSE;
		}
	}
	else
	{
		printf("PLUGIN: Failed to get VAD value.\n");
		return FALSE;
	}
}

void hlp_enableVad()
{
	if(ts3Functions.setPreProcessorConfigValue(connectionHandlerID, "vad", "true") == ERROR_ok)
	{
		printf("PLUGIN: VAD succesfully enabled.\n");
	}
	else
	{
		printf("PLUGIN: Failure enabling VAD.\n");
	}
}

void hlp_disableVad()
{
	if(ts3Functions.setPreProcessorConfigValue(connectionHandlerID, "vad", "false") == ERROR_ok)
	{
		printf("PLUGIN: VAD succesfully disabled.\n");
	}
	else
	{
		printf("PLUGIN: Failure disabling VAD.\n");
	}
}

void hlp_enableMic()
{
	if(ts3Functions.setClientSelfVariableAsInt(connectionHandlerID, CLIENT_INPUT_DEACTIVATED, INPUT_ACTIVE) == ERROR_ok)
	{
		int errorCode = ts3Functions.flushClientSelfUpdates(connectionHandlerID, NULL);
		if(errorCode == ERROR_ok || errorCode == ERROR_ok_no_update)
		{
			printf("PLUGIN: Microphone now enabled\n");
		}
		else
		{
			printf("PLUGIN: Failed to flush client self updates. Error code: %d\n", errorCode);
		}
	}
	else
	{
		printf("PLUGIN: Failed to set INPUT_ACTIVE client variable.\n");
	}
}

void hlp_disableMic()
{
	if(ts3Functions.setClientSelfVariableAsInt(connectionHandlerID, CLIENT_INPUT_DEACTIVATED, INPUT_DEACTIVATED) == ERROR_ok)
	{
		int errorCode = ts3Functions.flushClientSelfUpdates(connectionHandlerID, NULL);
		if(errorCode == ERROR_ok || errorCode == ERROR_ok_no_update)
		{
			printf("PLUGIN: Microphone now disabled\n");
		}
		else
		{
			printf("PLUGIN: Failed to flush client self updates. Error code: %d\n", errorCode);
		}
	}
	else
	{
		printf("PLUGIN: Failed to set INPUT_DEACTIVATED client variable.\n");
	}
}

void hlp_sendPluginCommand(string &commandText, anyID &idClient, BOOL isBroadcast)
{
	if(isBroadcast == TRUE)
	{
		ts3Functions.sendPluginCommand(connectionHandlerID, pluginID, commandText.c_str(), PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);
		printf("PLUGIN: Sent a broadcast message.\n");
	}
	else
	{
		ts3Functions.sendPluginCommand(connectionHandlerID, pluginID, commandText.c_str(), PluginCommandTarget_CLIENT, &idClient, NULL);
		printf("PLUGIN: Sent a message to specific client %d\n", idClient);
	}
}

BOOL hlp_majorSelfDataChange()
{
	// Checks if a major data change has occured.
	if(self->vehId != oldSelf->vehId		|| self->isOut != oldSelf->isOut		||
		self->kvChan0 != oldSelf->kvChan0	|| self->kvChan1 != oldSelf->kvChan1	|| 
		self->kvChan2 != oldSelf->kvChan2	|| self->kvChan3 != oldSelf->kvChan3	|| 
		self->kvActive != oldSelf->kvActive || self->kvSide != oldSelf->kvSide		||
		self->dvChan0 != oldSelf->dvChan0	|| self->dvChan0 != oldSelf->dvChan1	|| 
		self->dvChan0 != oldSelf->dvChan2	|| self->dvChan0 != oldSelf->dvChan3	||
		self->dvActive != oldSelf->dvActive || self->dvSide != oldSelf->dvSide		)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

int hlp_getChannel(anyID idClient, float clientArray[], float selfArray[])
{
	int channel;

	for(channel = 0; channel < 4; channel++)
	{
		if(clientArray[players[idClient].kvActive] == selfArray[channel] && selfArray[channel] != 0.0f)
		{
			return channel;
		}
	}
	return -1;
}

void hlp_timerThread(void* pArguments)
{
	int ticks = 0;

	while(stopRequested != TRUE)
	{
		if(timerReset == TRUE)
		{
			ticks = 0;
			timerReset = FALSE;
		}

		if(ticks > 5 && inRt == TRUE)
		{
			chnl_moveFromRt();
			timerReset = TRUE;
		}

		ticks++;
		Sleep(1000);
	}

	CloseHandle(timerThreadHndl);
	timerThreadHndl = INVALID_HANDLE_VALUE;
}

void hlp_setMetaData(string data)
{
	if(connected == TRUE)
	{
		if(ts3Functions.setClientSelfVariableAsString(connectionHandlerID, CLIENT_META_DATA, data.c_str()) == ERROR_ok)
		{
			printf("PLUGIN: Metadata set.\n");

			if(ts3Functions.flushClientSelfUpdates(connectionHandlerID, NULL) == ERROR_ok)
			{
				printf("PLUGIN: Metadata flush success.\n");
			}
			else
			{
				printf("PLUGIN: Metadata flush failure.\n");
			}
		}
		else
		{
			printf("PLUGIN: Failed to set metadata.\n");
		}
	}
	else
	{
		printf("PLUGIN: Not connected to a server. Skipping metadata update.\n");
	}
}

string hlp_generateMetaData()
{
	string pipeState;

	if(pipeOpen == TRUE)
		pipeState = "Yes";
	else
		pipeState = "No";

	return string(ts3plugin_name()) + string(" ") + string(ts3plugin_version()) + string("\nARMA 2 connected: ") + pipeState; 
}
/********************************* Helper Functions END ***************************************/

/********************************* Command Parsing & Processing Functions START ****************************/
void prs_commandText(string &commandText, anyID &idClient)
{
	int parseCode = commandCheck(commandText, *self, *other, *miniOther);

	if(parseCode != 1)
	{
		if(inRt == FALSE)
		{
			chnl_moveToRt();
		}
		timerReset = TRUE;
	}

	switch(parseCode)
	{
	case 0:
		printf("PARSER: Unknown command.\n");
		break;
	case 1:
		printf("PARSER: Corrupted command text. Failure of regexp validation.\n");
		break;
	case 10:
		printf("PARSER: POS command parsed.\n");
		prs_parsePOS();
		break;
	case 102:
		printf("PARSER: Failure parsing POS command.\n");
		break;
	case 11:
		printf("PARSER: OTH command parsed.\n");
		prs_parseOTH(idClient);
		break;
	case 112:
		printf("PARSER: Failure parsing OTH command.\n");
		break;
	case 12:
		printf("PARSER: MIN command parsed.\n");
		prs_parseMIN(idClient);
		break;
	case 122:
		printf("PARSER: Failure parsing MIN command.\n");
		break;
	case 13:
		printf("PARSER: REQ command parsed.\n");
		prs_parseREQ(idClient);	
		break;
	}
}

void prs_parsePOS()
{
	string generatedMessage;

	// Check if we are newly joined.
	if(oldSelf == NULL)
	{
		// Inform everyone of our position and request they do the same.
		msg_generateOTH(generatedMessage);
		hlp_sendPluginCommand(generatedMessage, myId, TRUE);

		msg_generateREQ(generatedMessage);
		hlp_sendPluginCommand(generatedMessage, myId, TRUE);
	}
	else
	{
		if(hlp_majorSelfDataChange() == TRUE)
		{
			// Major data change. Inform all clients of it.
			msg_generateOTH(generatedMessage);
			hlp_sendPluginCommand(generatedMessage, myId, TRUE);

			// Recalculate hearable sw/lw for all clients.
			float selfSWArray[] = {self->kvChan0, self->kvChan1, self->kvChan2, self->kvChan3};
			float selfLWArray[] = {self->dvChan0, self->dvChan1, self->dvChan2, self->dvChan3};
			QHash<anyID, argsComOTH>::iterator i = players.begin();
			while(i != players.end())
			{
				float playerSWArray[] = {players[i.key()].kvChan0, players[i.key()].kvChan1, players[i.key()].kvChan2, players[i.key()].kvChan3};
				float playerLWArray[] = {players[i.key()].dvChan0, players[i.key()].dvChan1, players[i.key()].dvChan2, players[i.key()].dvChan3};

				players[i.key()].hearableKV = hlp_getChannel(i.key(), playerSWArray, selfSWArray);
				players[i.key()].hearableDV = hlp_getChannel(i.key(), playerLWArray, selfLWArray);

				i++;
			}
		}
		else
		{
			// Minor data change.
			msg_generateMIN(generatedMessage);
			hlp_sendPluginCommand(generatedMessage, myId, TRUE);
		}
	}
	// Set own position and update positions of all known clients.
	pos_self();

	// Check if microphone needs to be enabled or disabled.
	if(self->TAN != 0 && talking == FALSE)
	{
		talking = TRUE;

		// Disable VAD so as not to interfere with microphone functioning.
		vadEnabled = hlp_checkVad();
		if(vadEnabled == TRUE)
			hlp_disableVad();
		
		// Enable microphone.
		hlp_enableMic();
	}
	else if(self->TAN == 0 && talking == TRUE)
	{
		talking = FALSE;

		// Enable VAD if it was disabled.
		if(vadEnabled == TRUE)
			hlp_enableVad();

		hlp_disableMic();
	}

	// Save data from this iteration for future use.
	*oldSelf = *self;
}

void prs_parseOTH(anyID &idClient)
{
	// Check if a player with such an id exists
	if(players.contains(idClient))
	{
		// Renew his data.
		players[idClient].posX		= other->posX;
		players[idClient].posY		= other->posY;
		players[idClient].posZ		= other->posZ;
		players[idClient].Dir		= other->Dir;
		players[idClient].Mode		= other->Mode;
		players[idClient].isOut		= other->isOut;
		players[idClient].vehId		= other->vehId;
		players[idClient].kvChan0	= other->kvChan0;
		players[idClient].kvChan1	= other->kvChan1;
		players[idClient].kvChan2	= other->kvChan2;
		players[idClient].kvChan3	= other->kvChan3;
		players[idClient].kvActive	= other->kvActive;
		players[idClient].kvSide	= other->kvSide;
		players[idClient].dvChan0	= other->dvChan0;
		players[idClient].dvChan1	= other->dvChan1;
		players[idClient].dvChan2	= other->dvChan2;
		players[idClient].dvChan3	= other->dvChan3;
		players[idClient].dvActive	= other->dvActive;
		players[idClient].dvSide	= other->dvSide;
		players[idClient].TAN		= other->TAN;
	}
	else
	{
		// Create a new record
		players.insert(idClient, *other);
		
		// Calculate if we can hear him.
		float playerSWArray[] = {players[idClient].kvChan0, players[idClient].kvChan1, players[idClient].kvChan2, players[idClient].kvChan3};
		float playerLWArray[] = {players[idClient].dvChan0, players[idClient].dvChan1, players[idClient].dvChan2, players[idClient].dvChan3};

		float selfSWArray[] = {self->kvChan0, self->kvChan1, self->kvChan2, self->kvChan3};
		float selfLWArray[] = {self->dvChan0, self->dvChan1, self->dvChan2, self->dvChan3};

		players[idClient].hearableKV = hlp_getChannel(idClient, playerSWArray, selfSWArray);
		players[idClient].hearableDV = hlp_getChannel(idClient, playerLWArray, selfLWArray);
	}
	pos_client(idClient);
}

void prs_parseMIN(anyID &idClient)
{
	// Check if this player is known to us.
	if(players.contains(idClient))
	{
		// Update variables.
		players[idClient].posX	= miniOther->posX;
		players[idClient].posY	= miniOther->posY;
		players[idClient].posZ	= miniOther->posZ;
		players[idClient].Dir	= miniOther->Dir;
		players[idClient].Mode	= miniOther->Mode;
		players[idClient].TAN	= miniOther->TAN;

		pos_client(idClient);
	}
	else
	{
		// We don't have full info on this person, and thus have to request it from him.
		string reqMessage;
		msg_generateREQ(reqMessage);
		hlp_sendPluginCommand(reqMessage, idClient, FALSE);
	}
}

void prs_parseREQ(anyID &idClient)
{
	// Generate an OTH message and reply to the sender of REQ message.
	string othMessage;
	msg_generateOTH(othMessage);
	hlp_sendPluginCommand(othMessage, idClient, FALSE);
}

/********************************* Command Parsing & Processing Functions END ******************************/

void RadioNoiseDSP(float slevel, short * samples, int sampleCount)
{
	float l = 1/(slevel+0.1f)/64;
	int i;
	for (i = 0; i < sampleCount; i++)
	{
		float d = (float)samples[i]/SHRT_MAX;
		float pdl;
		float pdh1;
		float pdh2;
		float k = (((float)rand()/RAND_MAX)*(1-slevel)*2+1);
		float n = ((float)rand()/RAND_MAX)*2-1;
		d *= k;
		//noise
		d += n*l;
		if (i > 0)
		{
			d = pdl + 0.15 * (d - pdl);
			pdl = d;
		}
		else
		{
			pdl = d;
		}
		if (i > 0)
		{
			float pd = d;
			d = 0.85 * (pdh1 + d - pdh2);
			pdh1 = d;
			pdh2 = pd;
		}
		else
		{
			pdh1 = d;
			pdh2 = d;
		}
		d *= 2;
		if (d > 1)
			d = 1;
		else if (d < -1)
			d = -1;
		samples[i] = d*(SHRT_MAX-1);
	}
}
