#ifndef NODE_PARALLEL_HPP
#define NODE_PARALLEL_HPP
#include <string>
#include <map>

#define DEFAULT_PORT 31415
#define DEFAULT_STATUS_INTERVAL 1000


enum ClientType{
	WORKER,
	CLIENT,
};

enum WorkerRequestType{
	NEW,
	DELETE,
	REQUEST_TASK,
	RUNING_TASK,
	TASK_FINISH,
	STILL_ALIVE,
};

enum ClientRequestType{
	NEW_TASK,
	CANCEL_TASK,
	STATUS_TASK,
};

typedef unsigned int WorkerID;
typedef unsigned long int JobID;

enum JobStatus{
	SUCESS,
	FAILED,
	PENDING,
	SUBMITED,
	RUNING,
	CANCEL,
};


inline std::multimap<std::string, std::string> argumentReader(int argc, char const *argv[])
{
	std::multimap<std::string, std::string> arg;
	int argic=0;
	while(argic<argc)
	{
		if (argv[argic][0]=='-')
		{
			bool minOne=false;
			int name=argic;
			argic++;
			while((argic<argc)&&(argv[argic][0]!='-'))
			{
				arg.insert(std::pair<std::string,std::string>(std::string(argv[name]),std::string(argv[argic])));
				argic++;
				minOne=true;
			}
			if(!minOne)arg.insert(std::pair<std::string,std::string>(std::string(argv[name]),std::string()));
		}
		else{
			argic++;
		}
	}
	return arg;
}


#endif // NODE_PARALLEL_HPP