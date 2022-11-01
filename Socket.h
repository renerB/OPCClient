#ifndef Socket_H
#define Socket_H

#include <WinSock2.h>

void sendDados_thread(SOCKET sock, char buf[50], int* nSeq, float* TempPreAq, float* TempAq, float* TempEnch, float* Vazao, char msgDados[], char msgAck[], int* status);;
void SocketMainThread(char* ipaddr, float* TempPreAq, float* TempAq, float* TempEnch, float* Vazao, float* SetTempPreAq, float* SetTempAq, int* SetTempEnch, int* key);
#endif // SIMPLE_OPC_CLIENT_H not defined#pragma once
