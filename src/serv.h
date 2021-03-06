#ifndef SSDB_SERVER_H_
#define SSDB_SERVER_H_

#include "include.h"
#include <map>
#include <vector>
#include <string>
#include "util/thread.h"
#include "ssdb.h"
#include "backend_dump.h"
#include "backend_sync.h"
#include "ttl.h"
#include "resp.h"
#include "proc.h"
#include "slave.h"

#define PROC_OK			0
#define PROC_ERROR		-1
#define PROC_THREAD     1
#define PROC_BACKEND	100

typedef std::vector<Bytes> Request;

class Server;

typedef int (*proc_t)(Server *serv, Link *link, const Request &req, Response *resp);

struct Command{
	static const int FLAG_READ		= (1 << 0);
	static const int FLAG_WRITE		= (1 << 1);
	static const int FLAG_BACKEND	= (1 << 2);
	static const int FLAG_THREAD	= (1 << 3);

	const char *name;
	const char *sflags;
	int flags;
	proc_t proc;
	uint64_t calls;
	double time_wait;
	double time_proc;
	// the position of key parameter in request
	// 0 or -1 for none
	int key_pos;
};

struct ProcJob{
	int result;
	Server *serv;
	Link *link;
	Command *cmd;
	double stime;
	double time_wait;
	double time_proc;
	
	ProcJob(){
		result = 0;
		serv = NULL;
		link = NULL;
		cmd = NULL;
		stime = 0;
		time_wait = 0;
		time_proc = 0;
	}
};


class Server{
	private:
		static const int READER_THREADS = 10;
		static const int WRITER_THREADS = 1;
		ProcMap proc_map;
	public:
		int link_count;
		SSDB *ssdb;
		BackendDump *backend_dump;
		BackendSync *backend_sync;
		ExpirationHandler *expiration;
		std::vector<Slave *> slaves;

		bool need_auth;
		std::string password;

		Server(SSDB *ssdb, const Config &conf);
		~Server();
		void proc(ProcJob *job);

		// WARN: pipe latency is about 20 us, it is really slow!
		class ProcWorker : public WorkerPool<ProcWorker, ProcJob>::Worker{
		public:
			ProcWorker(const std::string &name);
			~ProcWorker(){}
			void init();
			int proc(ProcJob *job);
		};
		WorkerPool<ProcWorker, ProcJob> *writer;
		WorkerPool<ProcWorker, ProcJob> *reader;
};

template<class T>
static std::string serialize_req(T &req){
	std::string ret;
	char buf[50];
	for(int i=0; i<req.size(); i++){
		if(i >= 5 && i < req.size() - 1){
			sprintf(buf, "[%d more...]", (int)req.size() - i);
			ret.append(buf);
			break;
		}
		if(((req[0] == "get" || req[0] == "set") && i == 1) || req[i].size() < 50){
			if(req[i].size() == 0){
				ret.append("\"\"");
			}else{
				std::string h = hexmem(req[i].data(), req[i].size());
				ret.append(h);
			}
		}else{
			sprintf(buf, "[%d]", (int)req[i].size());
			ret.append(buf);
		}
		if(i < req.size() - 1){
			ret.append(" ");
		}
	}
	return ret;
}

#endif
