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

void runWorker(WorkerID workerId, zmq::socket_t &socket, bool stopAfterComputation=false){
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
			memcpy(&jobId, reply.data(),sizeof(jobId));
			command.resize(reply.size()-sizeof(jobId));
			memcpy ((void*)command.data(),(char*)reply.data()+sizeof(jobId), command.length());
		}else{ // no job available, wait and retry
			if(stopAfterComputation){
				return;
			}else{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
		}

		if(command.empty())
			continue;

		char bashCmd[10]="/bin/bash";
		char comamndFlag[3]="-c";
		char* argv[4];

		argv[0]=bashCmd;
		argv[1]=comamndFlag;
		argv[2]=(char*)malloc(command.length()+1);
		strcpy(argv[2],command.c_str());
		argv[3]=nullptr;

		pid_t pid=fork(); // fork, to be crash resistant !!
		if (pid==0) { // child process //
			//std::string param[3]={"/bin/bash","-c",command};

			execv(argv[0], argv);
			std::cerr<< "failed" << std::endl;
			exit(127); // only if execv fails //
		}

		bool manuallyInterupted = false;

		requestType=RUNING_TASK;
		int status;
		unsigned char updateStatus=0;
		while((waitpid(pid, &status, WNOHANG) != pid) && !manuallyInterupted) {
		   if((updateStatus++%100)==0){
		   	usleep(300);
		   	continue;
		   }

		   zmq::message_t request (sizeof(clientType)+sizeof(requestType)+sizeof(workerId)+sizeof(jobId));
			memcpy((char*)request.data (), &clientType, sizeof(clientType));
			memcpy((char*)request.data ()+sizeof(clientType), &requestType, sizeof(requestType));
			memcpy((char*)request.data ()+sizeof(clientType)+sizeof(requestType), &workerId, sizeof(workerId));
			memcpy((char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType)+sizeof(workerId),&jobId,sizeof(jobId));
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
					#if DEBUG
						std::cerr<< "Cancelation requested" << std::endl;
					#endif
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

					if (!manuallyInterupted)
						{
							kill(pid, SIGKILL);
							waitpid(pid, &status, 0);
							manuallyInterupted = true;
						}
				}
			}
			
			
		}

		{
			requestType=TASK_FINISH;
			int jobReturnCode=0;
			if(WIFEXITED(status) || (manuallyInterupted && !needToCancel )){
				jobReturnCode=WEXITSTATUS(status);
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



	}
}

int main(int argc, char const *argv[]) {
	needToCancel=false;
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
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

	bool stopAfterComputation=false;
	if (arg.count("-sac") ==1)
	{
		stopAfterComputation=true;
	}
	arg.erase("-sac");

	std::string serverAddress="localhost";
	if (arg.count("-sa") ==1)
	{
		serverAddress=arg.find("-sa")->second;
	}
	arg.erase("-sa");

	std::stringstream commandStream;
	
	bool look4leftover=false;
	if (arg.count("-cmd") >= 1)
	{
		for (auto it=arg.equal_range("-cmd").first; it!=arg.equal_range("-cmd").second; ++it)
		{
			commandStream << it->second <<" ";
		}
		look4leftover=true;
	}
	arg.erase("-cmd");

	if(!asWorker && commandStream.str().empty())
	{
		std::string command;
		std::getline(std::cin, command);
		commandStream<<command;
	}

	if(look4leftover){
		for (auto it=arg.begin(); it!=arg.end(); ++it){
			commandStream << it->first <<" ";
			for (auto itloc=arg.equal_range(it->first).first; itloc!=arg.equal_range(it->first).second; ++itloc)
			{
				commandStream << itloc->second <<" ";
			}
		}
	}


	std::string command=commandStream.str();



	std::transform(serverAddress.begin(), serverAddress.end(), serverAddress.begin(),::tolower);

	//run daemon
	if(asWorker && runAsDaemon  && !needToCancel){
		fprintf(stderr, "start daemon\n" );
		bool stayInCurrentDirectory=true;
		bool dontRedirectStdIO=false;
		int value=daemon(stayInCurrentDirectory,dontRedirectStdIO);
		if(value==-1) fprintf(stderr, "fail to run daemon\n" );
		else fprintf(stderr, "Daemon started\n" );
	}

	if(needToCancel) return 0;
	zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REQ);
	if(needToCancel) return 0;
	char address[4096];
	sprintf(address,"tcp://%s:%d",serverAddress.c_str(),port);
	socket.connect (address);
	if(needToCancel) return 0;
	

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

		runWorker(workerId,socket,stopAfterComputation);


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
		//std::cout << command<< std::endl;
		ClientType clientType=CLIENT;
		JobStatus currentStatus=PENDING;
		
		JobID jobId=0;
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

		// wait for ouput 
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
				std::this_thread::sleep_for(std::chrono::milliseconds(25));
			}
		}	
	}
	

	return 0;
}