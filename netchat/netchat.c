#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define ROLE_CLIENT (0)
#define ROLE_SERVER (1)


// 1. message events 2. time out event 3. signal events,
#define MSG_TYPE_DETECT 0
#define MSG_TYPE_TALK   1
#define MSG_TYPE_SIGNAL 2

#define STAT_REQUEST (0)  //request
#define STAT_EST     (1)  //build connect
#define STAT_OFFLINE (2)  //offline

static int role = 0;
static int run = 1;
static unsigned short listen_port= 0;
static int sock = 0;

#define MEM_LEN_512 (512)
typedef struct{
	unsigned char msgType;
	unsigned int numOfrand;
	char body[256];
}msg_udpheader_t;
typedef struct{
    
    int flag;  //placeholder
    int fd;
    char ip[32];
    unsigned short port;	
    int status;//request link interrupt
    int role;  //·server or client  
    struct sockaddr_in sa;
}chat_session_t;




#define NUM (100)
static chat_session_t chat[NUM] = {{0}};



int add_session(char* ip, unsigned short port, int status, int role)
{
    int i  = 0;
    for(i = 0; i < NUM; i++){
        if (chat[i].flag == 0){
            strcpy(chat[i].ip, ip);

            chat[i].port = port;
            chat[i].status =status;
            chat[i].role = role;
            chat[i].flag = 1;
            return 0;
        }   
    }
    return -1;
}

chat_session_t* find_session(char* ip, unsigned short port)
{
    int i  = 0;
    for(i = 0; i < NUM; i++){
        if (chat[i].flag == 1){
            if (strcmp(ip, chat[i].ip) == 0 && chat[i].port == port){
               return &chat[i];
            }          
        }   
    }
    return 0;
}

//terminating all sessions...
void* thread_server(void* arg)
{
    struct sockaddr_in local;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	
	if(sock < 0){
	   printf("failed to call socket().\n");
	   exit(0);
	}
	
	local.sin_family=AF_INET;
	local.sin_port=htons(listen_port);
	local.sin_addr.s_addr = htonl(0);

	if(bind(sock,(struct sockaddr*)&local,sizeof(local))<0){
		printf("bind");
		return 0;
	}	
	
	while(run){		
		msg_udpheader_t head;
		struct sockaddr_in from;
		socklen_t len =sizeof(from);
		memset((char*)&from, 0, sizeof(from));
		
		ssize_t s = recvfrom(sock, (char*)&head, sizeof(head), 0, (struct sockaddr*)&from, &len);
		if (s <= 0){
            printf("Err:out\n");
			break;
		}        
	    printf("\nrecv new data:%d;%s\n",head.msgType,head.body);
		switch (head.msgType){
			case 0:
				//#session request from: 127.0.0.1 55833
				printf("#session request from: %s %d\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
				printf("#chat with?:\n");                
                add_session(inet_ntoa(from.sin_addr), ntohs(from.sin_port), STAT_REQUEST, ROLE_SERVER);                
				break;
			case 1:
                //#[127.0.0.1 41625] sent msg
				printf("[%s %d] send msg: %s\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), head.body);
				break;
			case 2:
				printf("2");
				break;
			default:
                printf("2");
				break;
		}		

	}
	return 0;

}


void* reader(void* arg) // read the thread
{
    if (arg == NULL){
        return 0;
    }
    
    chat_session_t* ses = (chat_session_t*)arg;        
	int n = 0;
socklen_t slen = sizeof(struct sockaddr);
    while(run){
        struct sockaddr_in from;
        msg_udpheader_t head;
        memset(&head, 0, sizeof(head));

    	n = recvfrom(ses->fd, &head, sizeof(head), 0, (struct sockaddr*)&from, &slen);
    	if (n <=0){
    		break;
    	}		
        //#[127.0.0.1 41625] sent msg: hello
    	printf("#[%s %d] send msg %s\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), head.body);
    	printf("chat:?");
    }

    return 0;
}

void response(char* ip, unsigned short port)
{
    socklen_t slen = sizeof(struct sockaddr_in);	     
    chat_session_t* ses = find_session(ip, port);
    if (port == 0){
        return;
    }
    if (ses == NULL){
        // to a specified server, there is only one session
        // a new session build need to wait the request accept
        //printf("connect to server %s:%d as a client\n", ip, port);
        int len = 0;
        int sockfd = 0;
        struct sockaddr_in sa;
    		

    	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            printf("Err:failed to call socket.\n");
    		return;
    	}

    	memset(&sa, 0, sizeof(sa));
    	sa.sin_family = AF_INET;
    	sa.sin_port = htons(port);
    	sa.sin_addr.s_addr = inet_addr(ip);
    	struct timeval tv_out;
    			 tv_out.tv_sec = 5;//time out 5 seconds
    			 tv_out.tv_usec = 0;
        //set the timeout of sock
        //if socket didn't recv any package from this listener ,the function recvfrom will retrun
    	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));

    	//If it is the first time, give three timeout intervals
    	int count = 0;	
    	while(count < 3){
    		msg_udpheader_t req;
    		req.msgType = MSG_TYPE_DETECT;		
    		
    		int res = sendto(sockfd, (char*)&req,  sizeof(req), 0, (struct sockaddr *)&sa, sizeof(struct sockaddr));
    		if (res <= 0){
    			printf("failed to send detect msg to peer.\n");
    			return;
    		}

    		struct sockaddr_in from;
    		res = recvfrom(sockfd, (char*)&req, sizeof(req), 0, (struct sockaddr *)&from, &slen);
    		if (res < 0){
    			count ++;
    			continue;			
    		}		
    		printf("#Success: %s %d\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
    		break;
    	}
    	
    	if (count >= 3){
    		printf("#failure: %s %d\n", ip, port);
    		return;
    	}
        
        add_session(ip, port, STAT_REQUEST, ROLE_CLIENT);
        ses = find_session(ip, port); 
        ses->fd = sockfd;
        memcpy((char*)&ses->sa, (char*)&sa, sizeof(sa));
        return;
    }
    
    if (ses == NULL){
        printf("ses is null,please check\n");
        return;
    } 

    //If it exists as a server, need to check whether it has been verified
    if (ses->role == ROLE_SERVER){
        if (ses->status == STAT_REQUEST){
            msg_udpheader_t msg;           
            memset((char*)&msg, 0, sizeof(msg));

            
            struct sockaddr_in to;
            memset(&to, 0, sizeof(to));
        	to.sin_family = AF_INET;
        	to.sin_port = htons(port);
        	to.sin_addr.s_addr = inet_addr(ip);
        	struct timeval tv_out;
        	tv_out.tv_sec = 5;//wait 5 seconds
        	tv_out.tv_usec = 0;

            //[127.0.0.1:41625] accept request? (y/n)y
            char c[10];
            int t1 =  time(0);
            printf("[%s:%d] accept request? (y/n)", ip, port);
            fgets(c,sizeof(c), stdin);
            if (c[0] != 'y'){
                return;
            }   
            
            printf("\n");
            
            int t2 = time(0);

            if (t2 - t1 >5){
                printf("warning: peer %s:%d might be offline; terminating session...\n", ip, port);                
                return ;
            }
            
            msg.msgType = MSG_TYPE_DETECT;
            int res = sendto(sock, (char*)&msg,  sizeof(msg), 0, (struct sockaddr *)&to, sizeof(struct sockaddr));;
            if (res <= 0){
                printf("failed to send msg to client\n");
                return;
            }
            
            memcpy(&ses->sa, &to, sizeof(struct sockaddr_in));
            ses->status = STAT_EST;//bulid connect
            return;
        }
    }
    
	reader(ses);    
	printf("[%s:%d] your message:",ip, port);
	msg_udpheader_t head;
	memset(&head, 0, sizeof(head));
    
	if(NULL == fgets(head.body, sizeof(head.body), stdin)){
		return;
	}    
    
	head.msgType = MSG_TYPE_TALK;    		
	if(sendto(ses->fd, (char*)&head,  sizeof(head), 0, (struct sockaddr *)&ses->sa, slen)<0) {
        printf("Err:failed to sendto");
	}else{
        printf("Success send msg to [%s:%d]\n",inet_ntoa(ses->sa.sin_addr), ntohs(ses->sa.sin_port));	
    }
}
void* thread_client(void* arg)
{
	while(run){
		char ip[32] = {0};
		char port[10] = {0};
		char tmp[MEM_LEN_512] = {0};

        
		printf("#chat with?:");
		if (NULL == fgets(tmp, sizeof(tmp), stdin)){
			break;
		}
        
        if (tmp[0] ==  '?'){
            int i = 0;
            for(i = 0; i <NUM;i++){
                if (chat[i].flag == 1){
                      printf("session list: %s %hu\n", chat[i].ip,chat[i].port);             

                }

            }
            continue;    
        }



		sscanf(tmp, "%s %s", ip, port);		        
        response(ip, atoi(port));
	}
	return 0;
}
void sig_handler(int signo)
{
  if (signo == SIGINT){
        printf("\nterminating all sessions...\n");
        close(sock);
        exit(0);
  }
  
  if (signo == SIGQUIT){
        printf("terminate session (for help <h>): ");        
        char tmp[128] = {0};
        fgets(tmp, sizeof(tmp), stdin);

        char ip[32] = {0};
        char port[10] = {0};

        sscanf(tmp, "%s %s",ip, port);
        printf("terminating session with %s", tmp);
        chat_session_t* ses = find_session(ip, atoi(port));
        if (ses != NULL){
            close(ses->fd);
            ses->flag = 0;
        }

  }
}
int main(int argc, char* argv[])
{
	if (2 != argc){
		printf("Usage: netchat port\n");
		return 0;
	}
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
	    printf("cannot catch SIGINT\n");
        exit(1);	
    }
    if (signal(SIGQUIT, sig_handler) == SIG_ERR) {
	    printf("cannot catch SIGINT\n");
        exit(1);	
    }
	listen_port = atoi(argv[1]);



	pthread_t tid;
	pthread_create(&tid, 0, thread_server, 0);
	pthread_create(&tid, 0, thread_client, 0);
    
	while(1){
		sleep(1000);
	}
	return 0;
}


