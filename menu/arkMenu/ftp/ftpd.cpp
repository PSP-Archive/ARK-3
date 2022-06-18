#include "common.h"
#include "controller.h"
#include "psp_main.h"
#include "std.h"
#include "util.h"
#include "ftp.h"
#include "ftpd.h"
#include "psp_cfg.h"
#include "sutils.h"
#include "psp_init.h"

  typedef struct thread_list {
    struct thread_list *next;
    int                 thread_id; 
  } thread_list;

  thread_list *mftp_thread_head = NULL;

  SOCKET sockListen = 0;

static void
mftpAddThread(int thread_id) 
{
  thread_list *new_thread = (thread_list *)malloc(sizeof(thread_list));
  new_thread->next      = mftp_thread_head;
  new_thread->thread_id = thread_id;

  mftp_thread_head = new_thread;
}

static void
mftpDelThread(int thread_id) 
{
  thread_list **prev_thread = &mftp_thread_head;
  thread_list  *del_thread;

  del_thread = mftp_thread_head; 
  while (del_thread != (thread_list *)0) {
    if (del_thread->thread_id == thread_id) break;
    prev_thread = &del_thread->next;
    del_thread  = del_thread->next;
  }
  if (del_thread) {
    *prev_thread = del_thread->next;
    free(del_thread);
  }
}

int
mftpExitHandler(SceSize argc, void *argv) 
{
  int err = 0;

  while (1) {
    Controller pad;
    pad.update();
    if (pad.decline()) break;
    sceKernelDelayThread(10000);
  }
  if (sockListen) {
	  err = sceNetInetClose(sockListen);
  }

  thread_list  *scan_thread = mftp_thread_head; 
  while (scan_thread != (thread_list *)0) {
    sceKernelTerminateThread(scan_thread->thread_id);
    scan_thread = scan_thread->next;
  }
	sceKernelExitDeleteThread(0);
  return 0;
}

char buffer[128];
void mftpDisplayStatusMessage();
void mftpDisplayBatteryMessage(void);

int 
mftpClientHandler(SceSize argc, void *argv) 
{
  int thid = sceKernelGetThreadId();
  mftpAddThread(thid);
	MftpConnection *con = *(MftpConnection **)argv;

	con->sockData =0;
	con->sockPASV =0;

  if (mftp_config.head_user) {
    strcpy(con->root,mftp_config.head_user->root);
  } else {
	  strcpy(con->root,"ms0:");
  }

	memset(con->sockCommandBuffer, 0, 1024);
	memset(con->sockDataBuffer, 0, 1024);
	strcpy(con->curDir,"/");
	memset(con->user, 0, MAX_USER_LENGTH);
	memset(con->pass, 0, MAX_PASS_LENGTH);
  strcpy(con->renameFromFileName,"");
  con->renameFrom = 0;
	con->usePassiveMode=0;
	con->userLoggedIn=0;
	con->port_port=0;
	con->port_addr[0] = 0;
	con->port_addr[1] = 0;
	con->port_addr[2] = 0;
	con->port_addr[3] = 0;
	con->transfertType='A';

	int err;

	mftpServerHello(con);

  char messBuffer[64];
	char readBuffer[1024];
	char lineBuffer[1024];
	int lineLen=0;
	int errLoop=0;
	while (errLoop>=0)
  {
  
    common::clearScreen(CLEAR_COLOR);
    sceDisplayWaitVblankStart();
    common::getImage(IMAGE_BG)->draw(0, 0);
    common::printText(0, 10, buffer);
    mftpDisplayStatusMessage();
    //mftpDisplayBatteryMessage();
    common::flipScreen();
  
  	int nb = sceNetInetRecv(con->sockCommand, (u8*)readBuffer, 1024, 0);
  	if (nb <= 0) break;

  	int i=0; 
  	while (i<nb) {
  		if (readBuffer[i]!='\r') {
  			lineBuffer[lineLen++]=readBuffer[i];
  			if (readBuffer[i]=='\n' || lineLen==1024) {
  				lineBuffer[--lineLen]=0;
  				char* command=skipWS(lineBuffer);
  				trimEndingWS(command);

          snprintf(messBuffer, 64, "> %s from %s", command, con->clientIp);
          mftpAddNewStatusMessage(messBuffer);

				  if ((errLoop=mftpDispatch(con,command))<0) break;
				  lineLen=0;
			  }
		  }
		  i++;
	  }
  }

	err = sceNetInetClose(con->sockCommand);
	free(con);

  mftpDelThread(thid);
	sceKernelExitDeleteThread(0);
	
	return 0;
}

int 
ftpdLoop(const char* szMyIPAddr)
{
  char buffer_2[64];
  u32 err;
  SOCKET sockClient;

  common::clearScreen(CLEAR_COLOR);

  common::getImage(IMAGE_BG)->draw(0, 0);



	char url[128];
	strcpy(url, "ftp://");
	strcat(url, szMyIPAddr);
	strcat(url, "/");

   strcpy(buffer, "FTP Server is now running on ");
   strcat(buffer, url);
   strcat(buffer, (common::getConf()->swap_buttons)? ", press X to exit !" : ", press () to exit !");
   common::printText(0, 10, buffer);

  int exit_id = sceKernelCreateThread("ftpd_client_exit",
                                      mftpExitHandler, 0x18, 0x10000, 0, 0);
	if(exit_id >= 0) {
		sceKernelStartThread(exit_id, 0, 0);
	}

	struct sockaddr_in addrListen;
	struct sockaddr_in addrAccept;
	u32 cbAddrAccept;
	sockListen = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
	if (sockListen & 0x80000000) goto done;
	addrListen.sin_family = AF_INET;
	addrListen.sin_port = htons(21);
	addrListen.sin_addr[0] = 0;
	addrListen.sin_addr[1] = 0;
	addrListen.sin_addr[2] = 0;
	addrListen.sin_addr[3] = 0;

	// any
	err = sceNetInetBind(sockListen, &addrListen, sizeof(addrListen));
	if (err) goto done;
	err = sceNetInetListen(sockListen, 1);
	if (err) goto done;


  common::printText(0, 20,"Waiting for FTP clients");

  if (mftp_config.auth_required) {

    common::printText(0, 30,"User authentication required");

  } else {

    common::printText(0, 30,"Anonymous connection mode");

  }

  

  common::flipScreen();



  while (1) {
  
	  cbAddrAccept = sizeof(addrAccept);

	  sockClient = sceNetInetAccept(sockListen, &addrAccept, (int*)&cbAddrAccept);
	  if (sockClient & 0x80000000) goto done;

    MftpConnection* con=(MftpConnection*)malloc(sizeof(MftpConnection));
    if (sceNetApctlGetInfo(8, (SceNetApctlInfo*)con->serverIp) != 0) {
      goto done;
    }

    snprintf(con->clientIp, 32, "%d.%d.%d.%d",
            addrAccept.sin_addr[0], addrAccept.sin_addr[1],
            addrAccept.sin_addr[2], addrAccept.sin_addr[3]);
    snprintf(buffer_2, 64, "Connection from %s", con->clientIp);
    mftpAddNewStatusMessage(buffer_2);

    con->sockCommand = sockClient;
	  int client_id = sceKernelCreateThread("ftpd_client_loop", mftpClientHandler, 0x18, 0x10000, 0, 0);
	  if(client_id >= 0) {
		  sceKernelStartThread(client_id, 4, &con);
	  }
	sceKernelWaitThreadEnd(client_id, 0);
  }

done:
	err = sceNetInetClose(sockListen);

  return 0;
}
