#include <iostream>
#include <zmq.hpp>
#include <cstdlib>
#include <string>
#include <thread>
#include "nodeParallel.hpp"
#include <csignal>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

bool needToCancel=false;

void signalHandler( int signum ) {
   needToCancel=true;  
}

void printHelp(){
	printf ("The help should definitely be written! :)\n");
}

void runWorker(WorkerID workerId, zmq::socket_t &socket){
	bool needToStop=false;
	ClientType clientType=WORKER;
	while(!needToStop && !needToCancel) {
		//look for a job
		WorkerRequestType requestType=REQUEST_TASK;

		zmq::message_t request (sizeof(clientType)+sizeof(requestType)+sizeof(workerId));
		memcpy((char*)request.data (), &clientType, sizeof(clientType));
		memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
		memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType), &workerId, sizeof(workerId));

		if(!socket.send (request,zmq::send_flags::none) ){
			//issue in sending
		}
		zmq::message_t reply;
		if(!socket.recv (reply) ){
			//issue in reciving 
		}

		JobID jobId;
		std::string command;
		if(reply.size()>=sizeof(jobId))
		{
			memcpy(&jobId, reply.data(),  sizeof(jobId));
			command.resize(reply.size()-sizeof(jobId));
			memcpy ((void*)command.data(),(char*)reply.data()+sizeof(jobId), command.length());
		}else{ // no job available, wait and retry
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if(command.empty())
			continue;
#if DEBUG
				std::cerr<< __LINE__ << std::endl;
			#endif
		pid_t pid=fork(); // fork, to be crash resistant !!
		if (pid==0) { // child process //
			std::string param[3]={"bash","-c",command};
			char bashCmd[50]="/bin/sleep";
			char comamndFlag[50]="10";
			char* argv[2];
			// for (int i = 0; i < 3; ++i)
			// {
			// 	argv[i]=(char*)param[i].c_str();
			// 	fprintf(stderr, "%s\n",argv[i] );
			// }
			argv[0]=bashCmd;
			argv[1]=comamndFlag;
			execv(argv[0], argv);
			exit(127); // only if execv fails //
		}
#if DEBUG
				std::cerr<< __LINE__ << std::endl;
			#endif
		bool manuallyInterupted = false;

		requestType=RUNING_TASK;
		int status;
		usleep(300);
		while(waitpid(pid, &status, WNOHANG) != pid) {
		    zmq::message_t request (sizeof(clientType)+sizeof(requestType)+sizeof(workerId));
			memcpy((char*)request.data (), &clientType, sizeof(clientType));
			memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType), &workerId, sizeof(workerId));
			if(!socket.send (request,zmq::send_flags::none) ){
				//issue in sending
			}
			zmq::message_t reply;
			if(!socket.recv (reply) ){
				//issue in reciving 
			}
			bool answer;
			if(reply.size()>=sizeof(answer))
			{
				
				memcpy ( &answer,reply.data (), sizeof(answer));
				if(!answer || needToCancel)
				{
					kill(pid, SIGTERM);

					for (int loop=0; !manuallyInterupted && loop < 6 ; ++loop)
					{
						int status;
						usleep(300);
						if (waitpid(pid, &status, WNOHANG) == pid) manuallyInterupted = true;
					}

					if (!manuallyInterupted) break;

					kill(pid, SIGINT);

					for (int loop=0; !manuallyInterupted && loop < 6 ; ++loop)
					{
						int status;
						usleep(300);
						if (waitpid(pid, &status, WNOHANG) == pid) manuallyInterupted = true;
					};

					if (!manuallyInterupted) kill(pid, SIGKILL);
				}
			}
			usleep(300);
			
		}
#if DEBUG
				std::cerr<< __LINE__ << std::endl;
			#endif
		{
			requestType=TASK_FINISH;
			int jobReturnCode=0;
			if(WIFEXITED(status) || (manuallyInterupted && !needToCancel )){
				//nothing particular 
		    }else{
			    if(WIFSIGNALED(status)){
			    	jobReturnCode=WTERMSIG(status);
			    }
		    }
			#if DEBUG
				std::cerr<<WIFEXITED(status) <<" return code :"<< jobReturnCode << std::endl;
			#endif
			zmq::message_t request (sizeof(clientType)+sizeof(requestType)+sizeof(workerId)+sizeof(jobId)+sizeof(jobReturnCode));
			memcpy((char*)request.data (), &clientType, sizeof(clientType));
			memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType), &workerId, sizeof(workerId));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType)+sizeof(workerId), &jobId, sizeof(jobId));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType)+sizeof(workerId)+sizeof(jobId), &jobReturnCode, sizeof(jobReturnCode));

			if(!socket.send (request,zmq::send_flags::none) ){
				//issue in sending
			}
			zmq::message_t reply;
			if(!socket.recv (reply) ){
				//issue in reciving 
			}
			bool answer;
			if(reply.size()>=sizeof(answer))
			{
				
				memcpy ( &answer,reply.data (), sizeof(answer));
				if(!answer)
				{
					// soemthing really weird is going on
				}
			}
		}
#if DEBUG
				std::cerr<< __LINE__ << std::endl;
			#endif


	}
}

int main(int argc, char const *argv[]) {
	needToCancel=false;
	std::multimap<std::string, std::string> arg=argumentReader(argc,argv);

	if ((arg.count("-h") == 1)|| (arg.count("--help") == 1))
	{
		printHelp();
		return 0;
	}
	arg.erase("-h");


	unsigned port=DEFAULT_PORT;
	if (arg.count("-p") ==1)
	{
		port=atoi(arg.find("-p")->second.c_str());
	}
	arg.erase("-p");

	std::string idName="";
	if (arg.count("-id") ==1)
	{
		idName=arg.find("-id")->second;
	}
	arg.erase("-id");

	bool runAsDaemon=false;
	if (arg.count("-d") ==1)
	{
		runAsDaemon=true;
	}
	arg.erase("-d");

	bool asWorker=false;
	if (arg.count("-w") ==1)
	{
		asWorker=true;
	}
	arg.erase("-w");

	bool blocking=false;
	if (arg.count("-b") ==1)
	{
		blocking=true;
	}
	arg.erase("-b");

	std::string serverAddress="localhost";
	if (arg.count("-sa") ==1)
	{
		serverAddress=arg.find("-sa")->second;
	}
	arg.erase("-sa");

	std::string command;
	if (arg.count("-cmd") ==1)
	{
		command=arg.find("-cmd")->second;
	}
	arg.erase("-cmd");

	if(!asWorker && command.empty())
	{
		std::getline(std::cin, command);
	}



	std::transform(serverAddress.begin(), serverAddress.end(), serverAddress.begin(),::tolower);

	//run daemon
	if(asWorker && runAsDaemon ){
		fprintf(stderr, "start daemon\n" );
		bool stayInCurrentDirectory=true;
		bool dontRedirectStdIO=false;
		int value=daemon(stayInCurrentDirectory,dontRedirectStdIO);
		if(value==-1) fprintf(stderr, "fail to run daemon\n" );
		else fprintf(stderr, "Daemon started\n" );
	}


	zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REQ);

	char address[4096];
	sprintf(address,"tcp://%s:%d",serverAddress.c_str(),port);
	socket.connect (address);

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	std::cerr<< __LINE__ << std::endl;
	if(asWorker){
		ClientType clientType=WORKER;
		WorkerRequestType requestType;
		WorkerID workerId=0;
		requestType=NEW;
		{
			zmq::message_t request (sizeof(clientType)+sizeof(requestType));
			memcpy((char*)request.data (), &clientType, sizeof(clientType));
			memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
			
			if(!socket.send (request,zmq::send_flags::none) ){
				//issue in sending
			}
			zmq::message_t reply;
			if(!socket.recv (reply) ){
				//issue in reciving 
			}

			if(reply.size()>=sizeof(workerId))
			{
				memcpy(&workerId, reply.data(), sizeof(workerId));
			}
			#if DEBUG
				std::cerr<< "worker register as:"<< workerId << std::endl;
			#endif
		}

#if DEBUG
				std::cerr<< __LINE__ << std::endl;
			#endif
		runWorker(workerId,socket);

		#if DEBUG
				std::cerr<< __LINE__ << std::endl;
			#endif

		requestType=DELETE;
		{
			zmq::message_t request (sizeof(clientType)+sizeof(requestType)+ sizeof(workerId));
			memcpy((char*)request.data (), &clientType, sizeof(clientType));
			memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType), &workerId, sizeof(workerId));

			if(!socket.send (request,zmq::send_flags::none) ){
				//issue in sending
			}
			zmq::message_t reply;
			if(!socket.recv (reply) ){
				//issue in reciving 
			}

			bool answer;
			if(reply.size()>=sizeof(answer))
			{
				
				memcpy ( &answer,reply.data (), sizeof(answer));
				if(!answer){
					fprintf(stderr, "Remove worker failed! :(\n");
				}
			}
		}
	}
	else{
		ClientType clientType=CLIENT;
		JobStatus currentStatus=PENDING;
		
		JobID jobId;
		// sumbit command	
		{
			ClientRequestType requestType=NEW_TASK;
			zmq::message_t request (sizeof(clientType)+sizeof(requestType)+command.length());
			memcpy((char*)request.data (), &clientType, sizeof(clientType));
			memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType), command.data(), command.length());
			if(!socket.send (request,zmq::send_flags::none) ){
				//issue in sending
			}
			zmq::message_t reply;
			if(!socket.recv (reply) ){
				//issue in reciving 
			}

			if(reply.size()>=sizeof(jobId))
			{
				memcpy(&jobId, reply.data(), sizeof(jobId));
			}
		}	

		// wiat for ouput 
		while (blocking) {
			ClientRequestType requestType;
			if(needToCancel){
				requestType=CANCEL_TASK;
			}else{
				requestType=STATUS_TASK;
			}
			zmq::message_t request (sizeof(clientType)+sizeof(requestType)+sizeof(jobId));
			memcpy((char*)request.data (), &clientType, sizeof(clientType));
			memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType), &jobId, sizeof(jobId));
			
			if(!socket.send (request,zmq::send_flags::none) ){
				//issue in sending
			}
			zmq::message_t reply;
			if(!socket.recv (reply) ){
				//issue in reciving 
			}

			if(requestType==CANCEL_TASK){
				bool answer;
				memcpy ( &answer,reply.data (), sizeof(answer));
				if(answer)
					break;
				else{
					fprintf(stderr, "Cancel failed! :(\n");
				}
			}else{
				memcpy (&currentStatus, reply.data (), sizeof(currentStatus));
				switch(currentStatus){		
					case PENDING:
					case SUBMITED:
					case RUNING:
					{		
						break;
					}
					case FAILED:
					{
						std::cerr<< "execution faild ! :(" << std::endl;
						return 1;
						break;
					}
					case CANCEL:
					case SUCESS:
					{	
						return 0;
						break;
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}	
	}
	

	return 0;
}