/* 
    ********************************************************************
    Odsek:          Elektrotehnika i racunarstvo
    Departman:      Racunarstvo i automatika
    Katedra:        Racunarska tehnika i racunarske komunikacije 
    Predmet:        Osnovi Racunarskih Mreza 1
    Godina studija: Treca (III)
    Skolska godina: 2019/2020
    Semestar:       Zimski (V)
    
    Ime fajla:      client.c
    Opis:           FTP klijent
    
    Platforma:      Raspberry Pi 2 - Model B
    OS:             Raspbian

    Autori: Ljubomir Avramović i Bojan Uljarević
    ********************************************************************
*/

#include<stdio.h>      //printf
#include <stdlib.h>
#include<string.h>     //strlen
#include<sys/socket.h> //socket
#include<arpa/inet.h>  //inet_addr
#include <fcntl.h>     //for open
#include <unistd.h>    //for close
#include<time.h>       //for random seed
#include <ifaddrs.h>   //for local ip resolution
#include <ctype.h>	   //tolower (for case insensitivity when entering commands)

#define DEFAULT_BUFLEN 1024
#define PACKET_BUFLEN 1448
#define DATA_PORT 20
#define COMMAND_PORT   21
#define COMMAND_OK 200
#define SERVICE_READY 220
#define TRANSFER_COMPLETE 226
#define LOGGED_IN 230
#define USER_OK_NEED_PASS 331


int commandSock, dataSock;
static char sendBuffer[DEFAULT_BUFLEN], 
            recvBuffer[DEFAULT_BUFLEN],  
            retrFilepath[DEFAULT_BUFLEN],
            storFilepath[DEFAULT_BUFLEN],
            retrBuffer[PACKET_BUFLEN],
            storBuffer[PACKET_BUFLEN],
            serverAddress[DEFAULT_BUFLEN];
typedef enum {STOR = 0, RETR, QUIT, UNRESOLVED} Command;


int FtpSendCommand(int socket, char* command, int len);
int FtpRecvResponse(int socket, char *response);
int FtpLogin(void);
int FtpPort(char* ipAddr, int* portNumber);
int FtpUpload();
int FtpDownload();
void CommandLoop(Command* command);
void EvaluateCommand(Command* command);
void FtpQuit(void);
int EvaluateResponse(void);
void cleanStdin(void);


int main(int argc , char *argv[])
{
  if (argc != 2) {
    printf("Usage: ./client ip_address)\n");
    return 1;
  }
  
    struct sockaddr_in server;

    //Create command socket
    commandSock = socket(AF_INET , SOCK_STREAM , 0);
    if (commandSock == -1)
    {
        printf("Could not create socket");
    }
    puts("Command socket created");

    strcpy(serverAddress, argv[1]);

    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons(COMMAND_PORT);

    //Connect to remote server
    if (connect(commandSock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return 1;
    }

    if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
    printf("Failed to receive a response.\n");
    }
    
    if(EvaluateResponse() != SERVICE_READY) 
    {
      printf("Couldn't establish connection.\n");
      FtpQuit();
      return 1;
    }

    //Enter username and password   
    if(FtpLogin() != 0) {
      printf("Login failed\n");
      FtpQuit();
      return 1;
    }
    
    //Choose to upload files, download files or quit program
    Command command; 
    CommandLoop(&command); 

    FtpQuit();

    return 0;
}


int FtpSendCommand(int socket, char* command, int len) {

  int bytesSent;
  if ((bytesSent = send(socket, command, len, 0)) < 0) { 
    return 1;
  }
  return 0;
}


int FtpRecvResponse(int socket, char *response)
{
  int bytesReceived;
	if ((bytesReceived = recv(socket, response, DEFAULT_BUFLEN, 0)) < 0) {
    return 1;
  }
  return 0;
}

int FtpLogin(void) {

  // USERNAME
  char username[50];
  memset(username, 0, 50);
  printf("Username: ");
  scanf("%s", username);
  memset(sendBuffer, 0, DEFAULT_BUFLEN);
  sprintf(sendBuffer, "USER %s\r\n", username);

  if (FtpSendCommand(commandSock, sendBuffer, strlen(sendBuffer)) != 0) {
    printf("Failed to send command.\n");
    return 1;
  }

  if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
    printf("Failed to receive a response.\n");
  }
  printf("%s\n", recvBuffer);

  if (EvaluateResponse() != USER_OK_NEED_PASS) return 1;
  memset(recvBuffer, 0, DEFAULT_BUFLEN);

  // PASSWORD
  char password[50];
  memset(password, 0, 50);
  printf("Password: ");
  scanf("%s", password);
  memset(sendBuffer, 0, DEFAULT_BUFLEN);
  sprintf(sendBuffer, "PASS %s\r\n", password);

  if (FtpSendCommand(commandSock, sendBuffer,strlen(sendBuffer)) != 0) {
    printf("Failed to send command.\n");
    return 1;
  }

  if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
    printf("Failed to receive a response.\n");
  }
  printf("%s\n", recvBuffer);

 if(EvaluateResponse() == LOGGED_IN) {
    printf("Logged in\n");
  } else return 1;
  
  memset(recvBuffer, 0, DEFAULT_BUFLEN);
  return 0;
}

int FtpPort(char* ipAddr, int* portNumber) {

  srand(time(0));
  unsigned short port = (rand() % (65535 + 1 - 49152)) + 49152;
  unsigned short portHigh =  port / 256;
  unsigned short portLow = port % 256;
  struct ifaddrs *addrs;
  getifaddrs(&addrs);
  struct ifaddrs *tmp = addrs;
  char ipBuffer[DEFAULT_BUFLEN];
  memset(ipBuffer, 0, DEFAULT_BUFLEN);

  while (tmp) 
  {
    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET)
    {
      struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
      if(strcmp(tmp->ifa_name, "lo") != 0) {
        strcpy(ipBuffer, inet_ntoa(pAddr->sin_addr));
        break;
      }
    }
    tmp = tmp->ifa_next;
  }
  freeifaddrs(addrs);

  int ipLength = strlen(ipBuffer);
  char ipAddress[ipLength];
  strcpy(ipAddress, ipBuffer);
  strcpy(ipAddr, ipAddress);
  int i;
  for(i = 0; i < ipLength; i++) {
    if(ipAddress[i] == '.') ipAddress[i] = ',';
  }
  
  sprintf(sendBuffer, "PORT %s,%d,%d\r\n", ipAddress, portHigh, portLow);

  if (FtpSendCommand(commandSock, sendBuffer, strlen(sendBuffer)) != 0) {
    printf("Failed to send command.\n");
    return 1;
  }
	
  if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
    printf("Failed to receive a response.\n");
    return 1;
  }
  printf("%s\n", recvBuffer);

  if(EvaluateResponse() != COMMAND_OK) return 1;
  memset(recvBuffer, 0, DEFAULT_BUFLEN);
  *portNumber = port;
  printf("Local port: %d\n", port);

  return 0;
}

int FtpUpload() {
    
    struct sockaddr_in client, server;
    char localAddress[32];
	int port;

    //Create data socket
    dataSock = socket(AF_INET , SOCK_STREAM , 0);
    if (dataSock == -1)
    {
        printf("Could not create socket");
        return 1;
    }
    puts("Data socket created");

    if (FtpPort(localAddress, &port) != 0) {
		puts("Port command failed");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }

    int localAddressLength = strlen(localAddress);
    int serverAddressLength = strlen(serverAddress);
    char locAddrTrim[localAddressLength];
    char srvAddrTrim[serverAddressLength];
    strcpy(locAddrTrim, localAddress);
    strcpy(srvAddrTrim, serverAddress);

    client.sin_addr.s_addr = inet_addr(locAddrTrim);
    client.sin_family = AF_INET;
    client.sin_port = htons(port);

    if( bind(dataSock,(struct sockaddr *)&client , sizeof(client)) < 0)
    {
        perror("bind failed. Error");
        close(dataSock);
		puts("Data socket closed");
        return 1;
    }

    listen(dataSock, 2);

    server.sin_addr.s_addr = inet_addr(srvAddrTrim);
    server.sin_family = AF_INET;
    server.sin_port = htons(DATA_PORT);
    
    printf("Enter filepath: ");
    scanf("%s", storFilepath);
    
    sprintf(sendBuffer, "STOR %s\r\n", storFilepath);
    if (FtpSendCommand(commandSock, sendBuffer, strlen(sendBuffer)) != 0) {
		printf("Failed to send command.\n");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }
    
    if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
		printf("Failed to receive a response.\n");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }
    printf("%s\n", recvBuffer);
    
    if(EvaluateResponse() != 150) {
		puts("Data transfer not allowed");
		close(dataSock);
		puts("Data socket closed");
		return 1;
	}
	
	memset(recvBuffer, 0, DEFAULT_BUFLEN);
	
    size_t c = sizeof(struct sockaddr_in);
    int serverSock = accept(dataSock, (struct sockaddr *)&server, (socklen_t*)&c);
    if (serverSock < 0)
    {
        perror("accept failed. Error");
        close(dataSock);
		puts("Data socket closed");
        return 1;
    }
    
    FILE* uploadFile = fopen(storFilepath, "rb");
    fseek(uploadFile, 0, SEEK_END);
    unsigned int fileSize = ftell(uploadFile);
    rewind(uploadFile);
    unsigned int bytesSent = 0, i, totalSent = 0;
    for(i = 0; i < fileSize; i += bytesSent) {
		bytesSent = fread(storBuffer, 1, sizeof(storBuffer), uploadFile);
		send(serverSock, storBuffer, bytesSent, 0);
		memset(storBuffer, 0, PACKET_BUFLEN);
		totalSent += bytesSent;
		printf("Bytes sent: %d\n", totalSent);	
	}
	printf("File size: %d\n", fileSize);
	fclose(uploadFile);
 

    close(dataSock);
    puts("Data socket closed");
    

  return 0;
}

int FtpDownload() {
  
    struct sockaddr_in client, server;
    char localAddress[32];
	int port;

    //Create data socket
    dataSock = socket(AF_INET , SOCK_STREAM , 0);
    if (dataSock == -1)
    {
        printf("Could not create socket");
        return 1;
    }
    puts("Data socket created");

    if (FtpPort(localAddress, &port) != 0) {
		puts("Port command failed");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }

    int localAddressLength = strlen(localAddress);
    int serverAddressLength = strlen(serverAddress);
    char locAddrTrim[localAddressLength];
    char srvAddrTrim[serverAddressLength];
    strcpy(locAddrTrim, localAddress);
    strcpy(srvAddrTrim, serverAddress);

    client.sin_addr.s_addr = inet_addr(locAddrTrim);
    client.sin_family = AF_INET;
    client.sin_port = htons(port);

    if( bind(dataSock,(struct sockaddr *)&client , sizeof(client)) < 0)
    {
        perror("bind failed. Error");
        close(dataSock);
		puts("Data socket closed");
        return 1;
    }

    listen(dataSock, 2);

    server.sin_addr.s_addr = inet_addr(srvAddrTrim);
    server.sin_family = AF_INET;
    server.sin_port = htons(DATA_PORT);
    
    printf("Enter filepath: ");
    scanf("%s", retrFilepath);
    
    sprintf(sendBuffer, "SIZE %s\r\n", retrFilepath);
    if (FtpSendCommand(commandSock, sendBuffer, strlen(sendBuffer)) != 0) {
		printf("Failed to send command.\n");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }
    
    if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
		printf("Failed to receive a response.\n");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }
        
    if(EvaluateResponse() != 213) {
		puts("File does not exist");
		close(dataSock);
		puts("Data socket closed");
		return 1;
	}
	int fileSize = atoi(recvBuffer + 4);
	memset(recvBuffer, 0, DEFAULT_BUFLEN);
    
    sprintf(sendBuffer, "RETR %s\r\n", retrFilepath);
    if (FtpSendCommand(commandSock, sendBuffer, strlen(sendBuffer)) != 0) {
		printf("Failed to send command.\n");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }
    
    if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
		printf("Failed to receive a response.\n");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }
    printf("%s\n", recvBuffer);
    
    if(EvaluateResponse() != 150) {
		puts("Data transfer not allowed");
		close(dataSock);
		puts("Data socket closed");
		return 1;
	}
	
	memset(recvBuffer, 0, DEFAULT_BUFLEN);
	
    size_t c = sizeof(struct sockaddr_in);
    int serverSock = accept(dataSock, (struct sockaddr *)&server, (socklen_t*)&c);
    if (serverSock < 0)
    {
        perror("accept failed. Error");
        close(dataSock);
		puts("Data socket closed");
        return 1;
    }
    
    FILE* downloadFile = fopen(retrFilepath, "wb");
    unsigned int bytesReceived = 0, i, totalReceived = 0;
    for(i = 0; i < fileSize; i += bytesReceived) {
		bytesReceived = recv(serverSock, retrBuffer, sizeof(retrBuffer), 0);
		fwrite(retrBuffer, 1, bytesReceived, downloadFile);
		memset(retrBuffer, 0, PACKET_BUFLEN);
		totalReceived += bytesReceived;
		printf("Bytes received: %d\n", totalReceived);	
	}
	printf("File size: %d\n", fileSize);
	fclose(downloadFile);
    
    if (FtpRecvResponse(commandSock, recvBuffer) != 0) {
		printf("Failed to receive a transfer response.\n");
		close(dataSock);
		puts("Data socket closed");
		return 1;
    }
    printf("%s\n", recvBuffer);
    
    if(EvaluateResponse() != 226) {
		puts("Data transfer not allowed");
		close(dataSock);
		puts("Data socket closed");
		return 1;
	}
	

    close(dataSock);
    puts("Data socket closed");
    

  return 0;
}

void CommandLoop(Command* command) {

  while(*command != QUIT) {
    printf(">> ");
    EvaluateCommand(command);
    switch(*command) {
      case STOR:
        if(FtpUpload() == 0) {
          printf("Upload successful\n");
        } else {
          printf("Upload failed\n");
        }
        break;
      case RETR:
        if(FtpDownload() == 0) {
          printf("Download successful\n");
        } else {
          printf("Download failed\n");
        }
        break;
      case UNRESOLVED: 
        printf("Commands: STOR / RETR / QUIT\n");
        break;
      default:
        break;
    }
  }

}

void EvaluateCommand(Command* command) {

  char commandBuffer[DEFAULT_BUFLEN];
  memset(commandBuffer, 0, DEFAULT_BUFLEN);
  scanf("%s", commandBuffer);
  char commandString[5];
  int i;
  for(i = 0; i < 4; i++) {
    commandString[i] = tolower(commandBuffer[i]);
  }
  commandString[4] = '\0';


  if(strcmp(commandString, "stor") == 0) {
    *command = STOR;
  } else if (strcmp(commandString, "retr") == 0) {
    *command = RETR;
  } else if(strcmp(commandString, "quit") == 0) {
    *command = QUIT;
  } else {
    *command = UNRESOLVED;
  }
  cleanStdin();
  memset(commandBuffer, 0, DEFAULT_BUFLEN);
}

void FtpQuit(void) 
{
    sprintf(sendBuffer, "QUIT\r\n");
	FtpSendCommand(commandSock, sendBuffer, strlen(sendBuffer));
    //FtpRecvResponse(commandSock, recvBuffer);
	close(commandSock);
	puts("Command socket closed");
}

int EvaluateResponse(void) {
  char response[4];
  int i;
  for(i = 0; i < 3; i++) response[i] = recvBuffer[i];
  response[3] = '\0';
  return atoi(response);
}

void cleanStdin(void)
{
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
}
