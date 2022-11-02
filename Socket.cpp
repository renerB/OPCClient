#define _CRT_SECURE_NO_WARNINGS /* Para evitar warnings sobre funçoes seguras de manipulacao de strings*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS /* para evitar warnings de funções WINSOCK depreciadas */

// Para evitar warnings irritantes do Visual Studio
#pragma warning(disable:6031)
#pragma warning(disable:6385)

// Bibliotecas:
#include <WinSock2.h>
#include <stdio.h>
#include <thread>
#include <chrono>
#include <Windows.h>
#include <mutex>

// Variáveis:
#define TamMsgDados 43 // 6+4+7+7+7+7 (mensagem) + 5 (separadores)
#define TamAck 11 // 6+4 (mensagem) + 1 (separador)
#define TamReqSet 11 // 6+4 (mensagem) + 1 (separador)
#define TamMsgSet 33 // 6+4+7+7+5 (mensagem) + 4 (separador)
#define TamAckSet 11 // 6+4 (mensagem) + 1 (separador)
#define port 3445

using namespace std;

mutex SeqMtx;
mutex ReadMtx;

SOCKET SetupSocket(SOCKET* oldsock, char* ipaddr) {
	closesocket(*oldsock);

	WSADATA wsaData;
	SOCKET newsock;
	SOCKADDR_IN ServerAddr;
	int status;
	
	while (true) {
		newsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (newsock == INVALID_SOCKET) {
			status = WSAGetLastError();
		}
		else {
			ServerAddr.sin_family = AF_INET;
			ServerAddr.sin_port = htons(port);
			ServerAddr.sin_addr.s_addr = inet_addr(ipaddr);

			// Estabelece a conexão com o servidor
			status = connect(newsock, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
			if (status == 0) {
				return newsock;
			}
			this_thread::sleep_for(chrono::milliseconds(2000));
		}
	}
	return NULL;
}

void sendDados_thread(SOCKET* sock, char* ipaddr, char buf[50], int* nSeq, float* TempPreAq, float* TempAq, float* TempEnch, float* Vazao, char msgDados[TamMsgDados + 1], char msgAck[TamAck + 1], int* status) {
	while (true) {
		memset(msgDados, 0, TamMsgDados + 1);
		SeqMtx.lock();
		ReadMtx.lock();
		sprintf(msgDados, "%06d;0000;%05.1f;%05.1f;%05.1f;%05.1f", *nSeq, *TempPreAq, *TempAq, *TempEnch, *Vazao);
		ReadMtx.unlock();
		*status = send(*sock, msgDados, TamMsgDados, 0);
		if (*status < 0) {
			*sock = SetupSocket(sock, ipaddr);
			*nSeq = 1;
			sprintf(msgDados, "%06d;0000;%05.1f;%05.1f;%05.1f;%05.1f", *nSeq, *TempPreAq, *TempAq, *TempEnch, *Vazao);
			*status = send(*sock, msgDados, TamMsgDados, 0);
		}

	// Recebimento de confirmação:
		memset(msgAck, 0, TamAck + 1);
		*status = recv(*sock, msgAck, TamAck, 0);
		if (*status < 0) {
			*sock = SetupSocket(sock, ipaddr);
			*nSeq = 1;
			SeqMtx.unlock();
		}
		else {
			buf = strtok(msgAck, ";");
			// Falta msg de erro e confrmação dos bits do Ack
			if (atoi(buf) != ((*nSeq) + 1)) {
				printf("Erro de sequenciamento - ACK");
			}
			*nSeq = atoi(buf) + 1;
			SeqMtx.unlock();
		}

		// Controle de temporiação de mensagem:
		this_thread::sleep_for(chrono::milliseconds(2000));
	}
}

void SocketMainThread(char* ipaddr, float* TempPreAq, float* TempAq, float* TempEnch, float* Vazao, float* SetTempPreAq, float* SetTempAq, int* SetTempEnch, int* key) {

	// Variáveis de conexão:
	WSADATA wsaData;
	SOCKET sock;
	SOCKADDR_IN ServerAddr;

	// Variáveis adicionais:
	char msgDados[TamMsgDados + 1];
	char msgAck[TamAck + 1];
	char msgReqSet[TamReqSet + 1];
	char msgSet[TamMsgSet + 1];
	char msgAckSet[TamAckSet + 1];
	char buf[50];
	int status;
	int nSeq = 1;

	// Inicialização WinSock versão 2.2:
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0) {
		printf("Falha na inicializacao do Winsock 2! Erro  = %d\n", WSAGetLastError());
	}

	// Criação de novo socket para estabelecer conexão:
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		status = WSAGetLastError();
		if (status == WSAENETDOWN)
			printf("Rede ou servidor de sockets inacessíveis!\n");
		else
			printf("Falha na rede: codigo de erro = %d\n", status);
	}

	// Inicializa a estrutura SOCKADDR_IN que será utilizada para
	// a conexão ao servidor.
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(port);
	ServerAddr.sin_addr.s_addr = inet_addr(ipaddr);

	// Estabelece a conexão com o servidor
	status = connect(sock, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (status == SOCKET_ERROR) {
		printf("Falha na conexao ao servidor ! Erro  = %d\n", WSAGetLastError());
	}

	std::thread t1(sendDados_thread, &sock, ipaddr, buf, &nSeq, TempPreAq, TempAq, TempEnch, Vazao, msgDados, msgAck, &status);

	while(true) {
		if (*key == 1) {
			memset(msgReqSet, 0, TamReqSet + 1);
			SeqMtx.lock();
			sprintf(msgReqSet, "%06d;1100", nSeq);
			status = send(sock, msgReqSet, TamReqSet, 0);
			if (status < 0) {
				sock = SetupSocket(&sock, ipaddr);
				nSeq = 1;
				sprintf(msgReqSet, "%06d;1100", nSeq);
				status = send(sock, msgReqSet, TamReqSet, 0);
			}

		// Recebimento de confirmação:
			memset(msgSet, 0, TamMsgSet + 1);
			status = recv(sock, msgSet, TamMsgSet, 0);
			if (status < 0) {
				sock = SetupSocket(&sock, ipaddr);
				nSeq = 1;
				SeqMtx.unlock();
			}
			else {
				sscanf(msgSet, "%6d", &nSeq);
				// Falta msg de erro e confrmação dos bits do Ack
				nSeq++;
				sprintf(msgAckSet, "%06d;0011", nSeq);
				status = send(sock, msgAckSet, TamReqSet, 0);
				if (status < 0) {
					sock = SetupSocket(&sock, ipaddr);
					nSeq = 1;
					sprintf(msgAckSet, "%06d;0011", nSeq);
					status = send(sock, msgAckSet, TamReqSet, 0);
				}
				nSeq++;
				SeqMtx.unlock();
				char buf1[5];
				char buf2[5];
				char buf3[5];

				for (int i = 0; i < TamMsgSet; i++) {
					if (i > 11 && i < 18) {
						buf1[i - 12] = msgSet[i];
					}
					if (i > 19 && i < 25) {
						buf2[i - 20] = msgSet[i];
					}
					if (i > 27 && i < 35) {
						buf3[i - 28] = msgSet[i];
					}
				}
				*SetTempPreAq = atof(buf1);
				*SetTempAq = atof(buf2);
				*SetTempEnch = atoi(buf3);
			}
			*key = 0;
		}
		else if (*key == 2)
		{
			closesocket(sock);
			exit(0);
		}

	}

}