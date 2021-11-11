
#include <iostream>
#include <zmq.hpp>
#include <map>
#include <queue>
#include <list>
#include <cstdlib>
#include <string>
#include <thread>
#include "nodeParallel.hpp"

void printHelp(){
	printf ("The help should definitely be written! :)\n");
}

typedef struct {
	std::map<WorkerID,std::list< JobID> >	worker2job;
	std::map<JobID,std::list<WorkerID>  >	job2worker;
	std::map<JobID,JobStatus>		job2status;
	std::map<JobID,std::string>		jobCommand;
	std::queue<JobID>				job2submit; //not really need put probably improuve performance
	std::map<WorkerID,time_t>		workerLastEvent;
	WorkerID						lastWorker;
	JobID							lastJob;
} JobManadger;

zmq::message_t manageClientMessage(zmq::message_t &request, JobManadger &jobManadger){
	zmq::message_t reply;
	size_t requesSize=request.size();
	if(requesSize>=sizeof(ClientType)+sizeof(ClientRequestType)){
		ClientRequestType requestType;
		memcpy(&requestType,(char*)request.data()+sizeof(ClientType),sizeof(ClientRequestType));
		JobID jobId;
		switch(requestType)
		{
			case NEW_TASK :
			{
				jobId=jobManadger.lastJob++;
				reply=zmq::message_t(sizeof(jobId));
				memcpy(reply.data (), &jobId, sizeof(jobId));
				std::string command;
				command.resize(requesSize-(sizeof(ClientType)+sizeof(ClientRequestType)));
				memcpy((void*)command.data(),(char*)request.data()+sizeof(ClientType)+sizeof(ClientRequestType),command.length());
				jobManadger.jobCommand[jobId]=command;
				jobManadger.job2status[jobId]=PENDING;
				jobManadger.job2submit.push(jobId);
				#if DEBUG
					std::cerr<< command<< std::endl;
				#endif
				break;
			}
			case CANCEL_TASK :
			{
				memcpy(&jobId,(char*)request.data()+sizeof(ClientType)+sizeof(ClientRequestType),sizeof(jobId));
				if(jobManadger.job2status[jobId]==PENDING){
					jobManadger.jobCommand.erase(jobId);
					jobManadger.job2status.erase(jobId);
				}else{
					jobManadger.job2status[jobId]=CANCEL;
				}
				bool answer=true;
				reply=zmq::message_t(sizeof(answer));
				memcpy (reply.data (), &answer, sizeof(answer));
				#if DEBUG
					std::cerr<< "client cancel task"<< std::endl;
				#endif
				break;
			}
			case STATUS_TASK :
			{
				memcpy(&jobId,(char*)request.data()+sizeof(ClientType)+sizeof(ClientRequestType),sizeof(jobId));
				JobStatus jobStatus=jobManadger.job2status[jobId];
				reply=zmq::message_t(sizeof(jobStatus));
				memcpy (reply.data (), &jobStatus, sizeof(jobStatus));
				#if DEBUG
					std::cerr<< "client check status"<< std::endl;
				#endif
				break;
			}
		}
	}
	return reply;
}

zmq::message_t manageWorkerMessage(zmq::message_t &request, JobManadger &jobManadger){
	zmq::message_t reply;
	size_t requesSize=request.size();
	if(requesSize>=sizeof(ClientType)+sizeof(WorkerRequestType)){
		WorkerRequestType requestType;
		memcpy(&requestType,(char*)request.data()+sizeof(ClientType),sizeof(WorkerRequestType));
		WorkerID workerId;
		switch(requestType)
		{
			case NEW :
			{
				workerId=jobManadger.lastWorker++;
				reply=zmq::message_t(sizeof(workerId));
				memcpy (reply.data (), &workerId, sizeof(workerId));
				#if DEBUG
					std::cerr<< "worker register"<< std::endl;
				#endif
				break;
			}
			case DELETE :
			{
				memcpy(&workerId,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType),sizeof(workerId));
				jobManadger.workerLastEvent.erase(workerId);
				bool answer=true;
				reply=zmq::message_t(sizeof(answer));
				memcpy (reply.data (), &answer, sizeof(answer));

				#if DEBUG
					std::cerr<< "worker unregister"<< std::endl;
				#endif
				break;
			}
			case REQUEST_TASK :
			{
				memcpy(&workerId,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType),sizeof(workerId));
				if(!jobManadger.job2submit.empty())
				{

					JobID jobId=jobManadger.job2submit.front();
	    			jobManadger.job2submit.pop();
	    			jobManadger.worker2job[workerId].push_back(jobId);
	    			jobManadger.job2worker[jobId].push_back(workerId);
					jobManadger.job2status[jobId]=SUBMITED;
					std::string command=jobManadger.jobCommand[jobId];
	#if DEBUG
					std::cerr<< __LINE__ << std::endl;
				#endif
					reply=zmq::message_t(sizeof(JobID)+command.length());
					memcpy (reply.data(), &jobId, sizeof(jobId));
					memcpy ((char*)reply.data()+sizeof(jobId), command.c_str(), sizeof(command));
					#if DEBUG
						std::cerr<< "worker request task and get one"<< std::endl;
					#endif
				}else{
					reply=zmq::message_t(0);
					#if DEBUG
						//std::cerr<< "worker request task but none available"<< std::endl;
					#endif
				}
				break;
			}
			case RUNING_TASK :
			{
				memcpy(&workerId,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType),sizeof(workerId));
				JobID jobId;
				memcpy(&jobId,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType)+sizeof(workerId),sizeof(jobId));
				bool answer=true;
				if(jobManadger.job2status[jobId]==CANCEL){
					answer=false;
				}else{
					jobManadger.job2status[jobId]=RUNING;
				}
				reply=zmq::message_t(sizeof(answer));
				memcpy (reply.data (), &answer, sizeof(answer));
				#if DEBUG
					std::cerr<< "worker infroma about runing task"<< std::endl;
				#endif
				break;
			}
			case TASK_FINISH :
			{
				memcpy(&workerId,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType),sizeof(workerId));
				JobID jobId;
				memcpy(&jobId,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType)+sizeof(workerId),sizeof(jobId));
				int jobReturnCode;
				memcpy(&jobReturnCode,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType)+sizeof(workerId)+sizeof(jobId),sizeof(jobReturnCode));
				if(jobReturnCode==0){
					jobManadger.job2status[jobId]=SUCESS;
					#if DEBUG
						std::cerr<< "get SUCESS"<< std::endl;
					#endif
				}else{
					jobManadger.job2status[jobId]=FAILED;
					#if DEBUG
						std::cerr<< "get FAILED"<< std::endl;
					#endif
				}
    			jobManadger.worker2job[workerId].remove(jobId);
    			jobManadger.job2worker[jobId].remove(workerId);
				jobManadger.jobCommand.erase(jobId);

				bool answer=true;
				reply=zmq::message_t(sizeof(answer));
				memcpy (reply.data (), &answer, sizeof(answer));
				#if DEBUG
					std::cerr<< "worker say task finished"<< std::endl;
				#endif
				break;
			}
			case STILL_ALIVE :
			{
				memcpy(&workerId,(char*)request.data()+sizeof(ClientType)+sizeof(WorkerRequestType),sizeof(workerId));
				bool answer=true;
				reply=zmq::message_t(sizeof(answer));
				memcpy (reply.data (), &answer, sizeof(answer));
				#if DEBUG
					std::cerr<< "worker still alive"<< std::endl;
				#endif
				break;
			}
		}
		if(requestType!=DELETE)
			time(&(jobManadger.workerLastEvent[workerId]));
	}
	return reply;
}


zmq::message_t manageNewMessage(zmq::message_t &request, JobManadger &jobManadger){
	size_t requesSize=request.size();
	zmq::message_t reply;
	if(requesSize>=sizeof(ClientType)){
		ClientType clientType;
		memcpy(&clientType,(char*)request.data(),sizeof(ClientType));
		switch(clientType)
		{
			case CLIENT :
			{
				reply=manageClientMessage(request, jobManadger);
				break;
			}
			case WORKER :
			{
				reply=manageWorkerMessage(request, jobManadger);
				break;
			}
		}
	}
	return reply;
};

int main(int argc, char const *argv[]) {
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

	//run daemon
	if(runAsDaemon ){
		fprintf(stderr, "start daemon\n" );
		bool stayInCurrentDirectory=true;
		bool dontRedirectStdIO=false;
		int value=daemon(stayInCurrentDirectory,dontRedirectStdIO);
		if(value==-1) fprintf(stderr, "fail to run daemon\n" );
		else fprintf(stderr, "Daemon started\n" );
	}

	bool needToStop=false;


	zmq::context_t context (1);
	
	zmq::socket_t receiver(context,ZMQ_REP);
	char address[1024];
	sprintf(address,"tcp://*:%d",port);

	try {
		receiver.bind(address);
	}
	catch(const std::exception& e) {
		std::cerr << e.what() << '\n';
		needToStop=true;
	}

	JobManadger jobManadger;
	jobManadger.lastWorker=1;
	jobManadger.lastJob=1;

	while (!needToStop) {
		zmq::message_t request;

		//  Check for next request from client
		auto reciveMessage=receiver.recv(request,zmq::recv_flags::dontwait);
		if( reciveMessage )
		{
			zmq::message_t reply=manageNewMessage(request,jobManadger);
			receiver.send(reply,zmq::send_flags::none);
		}else{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}	

	return 0;
}


